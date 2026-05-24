# Model options for the separator

Decision doc for replacing the placeholder `bandscnet.onnx` with a real vocal
separator. Status: **research only — no model integrated yet.** The committed
asset remains the spectral-gate placeholder.

## The core tension

The product goal (causal, streaming, **< 250 ms** latency) and "no training"
pull in opposite directions:

- **Causal / streaming models** that fit the latency goal — Band-SCNet (our
  original target), RT-STT (Nov 2025), HS-TasNet (2024) — **do not publish
  drop-in pretrained weights.** Using them still means training.
- **Pretrained, downloadable models** with strong quality — Spleeter,
  Open-Unmix, MDX-Net, Demucs — are **non-causal and segment-based.** They need
  a window of frames (often with look-ahead), so latency lands at ~0.5–1 s.
  That's "near-real-time," not streaming.

**There is no lightweight + pretrained + causal + ONNX-ready vocal model to drop
in.** Every path relaxes one constraint.

## Options

| Model | Domain / type | Latency class | Training? | ONNX | License | Notes |
| ----- | ------------- | ------------- | --------- | ---- | ------- | ----- |
| **Open-Unmix** `umx` / `umxl` | magnitude spectrogram → mask (BLSTM) | non-causal (~0.5–1 s) | none (MUSDB18 pretrained) | clean (PyTorch-native `*_spec`) | MIT | **Best fit for our seam** — pure magnitude domain, only the vocals target needed |
| **Spleeter** 2-stem | U-Net magnitude mask | non-causal (~0.5 s) | none | via `spleeter-pytorch` (TF1→PT→ONNX) | MIT | Compact, fast, proven; export is fiddlier (TF1 legacy ops) |
| **MDX-Net** (UVR) | frequency-domain mask | non-causal (chunked ~256 frames) | none | already distributed as ONNX | MIT-ish (per model) | Strong vocal quality; different chunked I/O to adapt; larger |
| **HTDemucs v4** | hybrid waveform | non-causal, high | none | community/GSoC 2025 effort | MIT | Best quality, too big/laggy for mobile |
| **Band-SCNet** | causal STFT mask | **streaming (~92 ms)** | **required** (no weights) | would export cleanly | — | Original target; matches the goal but needs training |
| **RT-STT / HS-TasNet** | causal | streaming | required (no public weights) | n/a | — | Right architecture class, same training problem |

## Recommendation (when we do build)

**Open-Unmix (`umxl`, vocals target only).** Rationale:

- It is a magnitude-spectrogram → magnitude model, which maps almost directly
  onto the existing `Separator` / `SpectrogramProcessor` seam. We derive a soft
  mask `vocal_mag / (mix_mag + ε)` and apply it to the complex spectrum we
  already compute, preserving phase.
- PyTorch-native → the cleanest ONNX export of the candidates (no TF1 legacy).
- MIT-licensed, MUSDB18-pretrained, only the single vocals target is needed
  (smaller than the 4-stem bundle).

Trade accepted: latency rises from one STFT window to one segment. The product
reframes as **near-real-time karaoke**, not low-latency streaming.

## What integration would change in this repo

The plumbing (capture, rings, FFT/STFT, inference thread, ONNX wiring) stays.
Only the model boundary changes from per-frame to per-segment:

- `Separator::process` operates on `[numBins × T frames]` instead of `[numBins]`.
- `SpectrogramProcessor` buffers `T` analysis frames and runs ONNX every
  `T·hop` samples (with segment overlap), raising algorithmic latency to ~`T·hop`.
- `OnnxSeparator` input/output tensors become `[1, numBins, T]` (or the model's
  native layout); the soft-mask derivation and complex-spectrum application are
  unchanged.
- `tools/export_bandscnet.py` gains an Open-Unmix export mode (load `umxl`
  vocals, trace the spectrogram core, export opset 17, optional FP16/INT8).

Size note: a single Open-Unmix target is tens of MB in FP32; INT8 quantization
brings it down materially. It still stacks on top of the ~19 MB ONNX Runtime
`.so`, so the < 20 MB APK goal needs the operator-reduced ORT build noted in the
README regardless of model choice.

## If the streaming goal is non-negotiable

Train a causal model (Band-SCNet or RT-STT). Practical route:

1. Reproduce the architecture from the paper.
2. Train on MUSDB18HQ (+ augmentation) on a GPU.
3. Optionally distill from a non-causal teacher (e.g. Open-Unmix/Demucs) into
   the small causal student to speed convergence.
4. Export with STFT outside the graph, FP16-quantize.

This is the only way to keep < 250 ms; there is no pretrained shortcut.

## Sources

- [Open-Unmix (sigsep)](https://sigsep.github.io/open-unmix/) ·
  [open-unmix-pytorch](https://github.com/sigsep/open-unmix-pytorch) ·
  [open-unmix-onnx export](https://github.com/KodeWorker/open-unmix-onnx)
- [Spleeter (Deezer)](https://github.com/deezer/spleeter) ·
  [spleeter-pytorch → ONNX/MNN](https://github.com/bigcash/spleeter-pytorch-mnn)
- [MDX-Net / on-device ONNX vocal separation](https://web.navan.dev/posts/2025-10-26-vocal-separation-and-rvc-onnx-coreml.html)
- [Demucs v4 → ONNX (GSoC 2025, Mixxx)](https://mixxx.org/news/2025-10-27-gsoc2025-demucs-to-onnx-dhunstack/)
- [Band-SCNet (Interspeech 2025)](https://www.isca-archive.org/interspeech_2025/yang25d_interspeech.pdf)
- [Towards Practical Real-Time Low-Latency MSS / RT-STT (arXiv 2511.13146)](https://arxiv.org/abs/2511.13146)
- [HS-TasNet (real-time hybrid spectrogram-TasNet)](https://arxiv.org/html/2402.17701v1)
