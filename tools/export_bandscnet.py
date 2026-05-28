#!/usr/bin/env python3
"""Export an ONNX separation model honoring the engine's per-frame contract.

The native ``OnnxSeparator`` feeds one analysis frame at a time:

    input  "magnitude" : float32[1, num_bins]   (per-bin magnitude)
    output "mask"      : float32[1, num_bins]    (per-bin soft mask in [0, 1])

and applies the mask to the complex spectrum, preserving phase.

This script produces a model with that exact I/O so the whole pipeline
(capture -> STFT -> ONNX -> mask -> iSTFT -> output) runs on-device. Two
architectures are available:

  --arch gate   (default) A tiny, real spectral-gate: mask = sigmoid(k*(mag-t)).
                Attenuates near-silent bins. NOT trained vocal isolation -- it is
                a placeholder that exercises the full ONNX path with audible,
                sensible behavior and a <2 KB file.

  --arch mlp    A small MLP mask head (the shape a trained Band-SCNet head would
                take), initialized near passthrough. Use as a starting point for
                wiring real weights.

Replacing this with a trained Band-SCNet requires training on MUSDB18HQ et al.
and moving the STFT/iSTFT outside the graph -- out of scope for this script.

Usage:
    python3 tools/export_bandscnet.py --arch gate \
        --out app/src/main/assets/bandscnet.onnx
"""
import argparse

import numpy as np
import onnx
from onnx import TensorProto, helper, numpy_helper

# fftSize 2048 -> 2048/2 + 1 bins. Must match kFftSize in AudioEngine.cpp.
DEFAULT_NUM_BINS = 1025


def build_gate(num_bins: int, threshold: float, slope: float) -> onnx.ModelProto:
    mag = helper.make_tensor_value_info("magnitude", TensorProto.FLOAT, [1, num_bins])
    mask = helper.make_tensor_value_info("mask", TensorProto.FLOAT, [1, num_bins])

    t = numpy_helper.from_array(np.array([threshold], dtype=np.float32), "t")
    k = numpy_helper.from_array(np.array([slope], dtype=np.float32), "k")

    nodes = [
        helper.make_node("Sub", ["magnitude", "t"], ["centered"]),
        helper.make_node("Mul", ["centered", "k"], ["scaled"]),
        helper.make_node("Sigmoid", ["scaled"], ["mask"]),
    ]
    graph = helper.make_graph(nodes, "spectral_gate", [mag], [mask], [t, k])
    return _finalize(graph)


def build_mlp(num_bins: int, hidden: int) -> onnx.ModelProto:
    rng = np.random.default_rng(0)
    w1 = (rng.standard_normal((num_bins, hidden)) * 0.01).astype(np.float32)
    b1 = np.zeros((hidden,), dtype=np.float32)
    w2 = (rng.standard_normal((hidden, num_bins)) * 0.01).astype(np.float32)
    # Large positive output bias -> sigmoid ~ 1 -> near passthrough when untrained.
    b2 = np.full((num_bins,), 6.0, dtype=np.float32)

    inits = [
        numpy_helper.from_array(w1, "w1"),
        numpy_helper.from_array(b1, "b1"),
        numpy_helper.from_array(w2, "w2"),
        numpy_helper.from_array(b2, "b2"),
    ]
    mag = helper.make_tensor_value_info("magnitude", TensorProto.FLOAT, [1, num_bins])
    mask = helper.make_tensor_value_info("mask", TensorProto.FLOAT, [1, num_bins])
    nodes = [
        helper.make_node("MatMul", ["magnitude", "w1"], ["h0"]),
        helper.make_node("Add", ["h0", "b1"], ["h1"]),
        helper.make_node("Relu", ["h1"], ["h2"]),
        helper.make_node("MatMul", ["h2", "w2"], ["o0"]),
        helper.make_node("Add", ["o0", "b2"], ["o1"]),
        helper.make_node("Sigmoid", ["o1"], ["mask"]),
    ]
    graph = helper.make_graph(nodes, "mlp_mask_head", [mag], [mask], inits)
    return _finalize(graph)


def _finalize(graph: onnx.GraphProto) -> onnx.ModelProto:
    model = helper.make_model(
        graph,
        opset_imports=[helper.make_opsetid("", 17)],
        producer_name="vocalremover-export",
    )
    model.ir_version = 9  # compatible with ONNX Runtime 1.23
    onnx.checker.check_model(model)
    return model


def verify(path: str, num_bins: int) -> None:
    """Load the model in ONNX Runtime and sanity-check its output."""
    import onnxruntime as ort

    sess = ort.InferenceSession(path, providers=["CPUExecutionProvider"])
    name = sess.get_inputs()[0].name
    mag = np.abs(np.random.default_rng(1).standard_normal((1, num_bins))).astype(np.float32)
    out = sess.run(None, {name: mag})[0]
    assert out.shape == (1, num_bins), out.shape
    assert np.all(out >= 0.0) and np.all(out <= 1.0), "mask must be in [0, 1]"
    print(f"verified: input '{name}' -> mask {out.shape}, "
          f"range [{out.min():.4f}, {out.max():.4f}]")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", default="app/src/main/assets/bandscnet.onnx")
    p.add_argument("--arch", choices=["gate", "mlp"], default="gate")
    p.add_argument("--num-bins", type=int, default=DEFAULT_NUM_BINS)
    p.add_argument("--threshold", type=float, default=0.02, help="gate: magnitude knee")
    p.add_argument("--slope", type=float, default=200.0, help="gate: transition slope")
    p.add_argument("--hidden", type=int, default=256, help="mlp: hidden units")
    args = p.parse_args()

    if args.arch == "gate":
        model = build_gate(args.num_bins, args.threshold, args.slope)
    else:
        model = build_mlp(args.num_bins, args.hidden)

    import os
    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    onnx.save(model, args.out)
    size_kb = os.path.getsize(args.out) / 1024.0
    print(f"wrote {args.out} ({args.arch}, {size_kb:.1f} KB)")
    verify(args.out, args.num_bins)


if __name__ == "__main__":
    main()
