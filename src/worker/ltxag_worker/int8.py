"""8-bit weights for the transformer (CVT port, cvt1.53: precision "int8", the node's Weights 8-bit).

Weight-only int8, one scale per output row (symmetric, |w| max / 127), for the Linears that hold
almost all of the transformer: attention q/k/v/out and the feed-forward pair in every block (the
same set upstream's fp8 cast uses, ltx_core/quantization/fp8_cast.py). Everything else stays in
16 bits, and the picture data always runs in 16 bits: only the stored weights are smaller.

How it is loaded, through upstream's own hooks:
  - an SDOps key-value operation quantizes each weight as it is read (bf16 from the file, one
    tensor at a time, so the bf16 copy never exists in full), adding `<name>.weight_scale`;
  - a ModuleOps turns those nn.Linear into Int8Linear on the meta model before loading;
  - a FuseRule fuses the IC-LoRA into the int8 weight: dequantize, add the LoRA delta in fp32,
    quantize again with a fresh scale.
Each scale is rounded to bf16 before the weights are divided by it, so the loader's cast of the
scales to bf16 (_cast_floating_sd) changes nothing.

Each call expands the layer's weight to 16 bits and uses the normal matmul (see Int8Linear).
"""

from __future__ import annotations

import torch

PREFIX = "transformer_blocks."
SUFFIXES = (".to_q", ".to_k", ".to_v", ".to_out.0", "ff.net.0.proj", "ff.net.2")
SCALE_DTYPE = torch.bfloat16


def is_int8_linear(module_name: str) -> bool:
    return module_name.startswith(PREFIX) and module_name.endswith(SUFFIXES)


ROWS = 1024  # rows per step when converting: keeps the fp32 scratch small (the MPS cache keeps it)


def quantize(w: torch.Tensor, delta: torch.Tensor | None = None) -> tuple[torch.Tensor, torch.Tensor]:
    """(int8 weight, per-row scale in SCALE_DTYPE) for a 2-D weight (plus `delta`, if given), in
    blocks of ROWS rows so no full-size fp32 copy is made."""
    q = torch.empty(w.shape, dtype=torch.int8, device=w.device)
    scale = torch.empty(w.shape[0], dtype=SCALE_DTYPE, device=w.device)
    for r in range(0, w.shape[0], ROWS):
        b = w[r:r + ROWS].float()
        if delta is not None:
            b += delta[r:r + ROWS].float()
        s = (b.abs().amax(dim=1) / 127.0).clamp_(min=1e-12).to(SCALE_DTYPE)
        q[r:r + ROWS] = torch.round(b / s.float()[:, None]).clamp_(-127, 127).to(torch.int8)
        scale[r:r + ROWS] = s
    return q, scale


def dequantize(q: torch.Tensor, scale: torch.Tensor, dtype: torch.dtype = torch.float32) -> torch.Tensor:
    return q.to(dtype) * scale.to(dtype)[:, None]


class Int8Linear(torch.nn.Linear):
    """nn.Linear with an int8 weight and a per-row scale (__class__ reassignment, as fp8_cast).
    The weight is expanded to the input's dtype for each call (one layer at a time, at most
    ~128 MB) and multiplied with the normal matmul: torch._weight_int8pack_mm was 2x slower on
    the whole job (it is made for a few tokens at a time; a chunk here has tens of thousands)."""

    weight_scale: torch.nn.Parameter

    def forward(self, input: torch.Tensor) -> torch.Tensor:  # noqa: A002
        w = dequantize(self.weight, self.weight_scale, input.dtype)
        return torch.nn.functional.linear(input, w, self.bias)


def _to_int8_linear(layer: torch.nn.Linear) -> None:
    layer.__class__ = Int8Linear
    dev = layer.weight.device
    layer.weight = torch.nn.Parameter(torch.empty(layer.weight.shape, dtype=torch.int8, device=dev),
                                      requires_grad=False)
    layer.weight_scale = torch.nn.Parameter(torch.empty(layer.weight.shape[0], dtype=SCALE_DTYPE, device=dev),
                                            requires_grad=False)


def convert_model(model: torch.nn.Module) -> torch.nn.Module:
    for name, m in model.named_modules():
        if isinstance(m, torch.nn.Linear) and is_int8_linear(name):
            _to_int8_linear(m)
    return model


def quantize_fused(q: torch.Tensor, scale: torch.Tensor, delta: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
    """int8 weight + a LoRA delta, quantized again, in blocks of ROWS rows."""
    out_q = torch.empty_like(q)
    out_s = torch.empty_like(scale)
    for r in range(0, q.shape[0], ROWS):
        bq, bs = quantize(dequantize(q[r:r + ROWS], scale[r:r + ROWS]), delta[r:r + ROWS])
        out_q[r:r + ROWS], out_s[r:r + ROWS] = bq, bs
    return out_q, out_s


def add_quantize_ops(sd_ops):  # noqa: ANN001, ANN201
    """sd_ops plus the load-time quantization of every int8 Linear weight."""
    from ltx_core.loader.sd_ops import KeyValueOperationResult

    count = [0]

    def op(key: str, value: torch.Tensor) -> list:
        q, s = quantize(value)
        if value.device.type == "mps":
            # The loader copies each bf16 tensor in without waiting (non_blocking), so without a
            # sync their memory stays held until the whole load ends (peak 37.5 GB vs 14.5 held).
            torch.mps.synchronize()
            count[0] += 1
            if count[0] % 40 == 0:
                torch.mps.empty_cache()
        return [KeyValueOperationResult(key, q), KeyValueOperationResult(key + "_scale", s)]

    for suffix in SUFFIXES:
        sd_ops = sd_ops.with_kv_operation(op, key_prefix=PREFIX, key_suffix=suffix + ".weight")
    return sd_ops


def policy():  # noqa: ANN201
    """The QuantizationPolicy (no sd_ops: those go into the model's own SDOps, see add_quantize_ops,
    because chaining a policy's sd_ops drops the video-only allowed_keys)."""
    from ltx_core.loader.fuse_loras import FuseRule, bf16_fuse_rule
    from ltx_core.loader.module_ops import ModuleOps
    from ltx_core.model.transformer.model import LTXModel
    from ltx_core.quantization.policy import QuantizationPolicy

    def fuse(key: str, weight: torch.Tensor, deltas: torch.Tensor, model_sd) -> dict:  # noqa: ANN001
        if weight.dtype != torch.int8:
            return bf16_fuse_rule(key, weight, deltas, model_sd)
        scale = model_sd.sd[key + "_scale"].to(weight.device)
        q, s = quantize_fused(weight, scale, deltas)
        return {key: q, key + "_scale": s}

    return QuantizationPolicy(
        sd_ops=None,
        module_ops=(ModuleOps(name="cvt_int8_weights", matcher=lambda m: isinstance(m, LTXModel),
                              mutator=convert_model),),
        fuse_rule=FuseRule(aggregation_dtype=torch.bfloat16, fuse_fn=fuse))
