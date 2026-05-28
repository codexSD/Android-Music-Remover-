# Vocal Remover (Android)

Real-time, on-device vocal isolation. The app captures another app's audio
(YouTube, YT Music, games, local players) via the **AudioPlaybackCapture** API,
runs it through a streaming source-separation model, and plays back the
vocals-only result through a low-latency **Oboe** stream.

All processing is on-device; **no audio ever leaves the phone.**

## Goal / definition of done

A shipped internal-testing build that, on a Snapdragon 8 Gen 2+ / Tensor G3+
class device:

- captures media audio and plays it back with **< 250 ms** end-to-end latency,
- suppresses instrumental content to a karaoke-grade degree using Band-SCNet,
- survives routing changes, rotation, and process death without crashing,
- stays within a **< 20 MB** APK and never transmits audio off-device.

Known hard limits (Android-enforced, no workaround): Spotify, DRM/Chrome audio,
and voice calls cannot be captured. Sub-100 ms latency and studio-grade
isolation are out of scope for a causal/streaming model.

## Roadmap

| Phase | Scope | Status |
| ----- | ----- | ------ |
| 1 — Capture pipeline | capture → ring buffer → Oboe playback | **done** |
| 2 — Model integration | STFT + ONNX separator behind the `AudioProcessor` seam | **done** (placeholder weights) |
| 3 — Stability & edge cases | routing, lifecycle, capture-blocked detection | **done** |
| 4 — Release prep | ABI splits, R8, Play compliance, privacy | **done** (size caveat below) |

**The one missing piece is trained weights.** The full pipeline runs, but the
bundled model is a spectral-gate placeholder, not a trained Band-SCNet. See
[Model](#model).

## Architecture

```
MediaProjection consent ─▶ CaptureService (foreground, mediaProjection type)
                               │ builds AudioRecord (PCM float, 48 kHz, stereo)
                               ▼
   ┌──────────────────── native AudioEngine (JNI) ────────────────────┐
   │ AudioRecordReader ─▶ ringIn ─▶ ProcessingThread ─▶ ringOut ─▶ Oboe │
   │   (capture thread)            (AudioProcessor)            (output) │
   │                                    │                              │
   │                     PassthroughProcessor | SpectrogramProcessor    │
   │                          STFT ─▶ Separator (ONNX) ─▶ iSTFT          │
   └───────────────────────────────────────────────────────────────────┘
```

Model inference runs on its own thread, never in the Oboe callback (which only
copies from `ringOut`). The two lock-free rings absorb scheduling jitter.

- **`:app`** — Kotlin UI/service: `MainActivity`, `CaptureService`,
  `CaptureStateMachine`, `CaptureHealthMonitor`.
- **`:audio`** — native engine + `AudioEngine` JNI wrapper. `AudioProcessor` is
  the DSP seam; `Separator` is the model seam (`IdentitySeparator`,
  `OnnxSeparator`).

## Model

The engine loads `app/src/main/assets/bandscnet.onnx` if present (else it runs
passthrough). The model contract is one analysis frame at a time:

```
input  "magnitude" : float32[1, 1025]   →   output "mask" : float32[1, 1025] in [0,1]
```

`tools/export_bandscnet.py` generates models honoring that contract. The
committed asset is a **spectral-gate placeholder** — a real ONNX graph that runs
on-device, but **not trained vocal isolation**. Shipping karaoke-grade
separation requires training Band-SCNet (MUSDB18HQ et al.), exporting with the
STFT outside the graph, and FP16-quantizing — out of scope for this repo, which
delivers the runnable plumbing the trained weights drop into.

## Building

Requires the Android SDK (API 35), NDK r26+, and CMake 3.22+. Point Gradle at
the SDK via `local.properties` (`sdk.dir=...`) or `$ANDROID_HOME`, then:

```bash
./gradlew :app:assembleDebug      # build the APK
./gradlew :app:assembleRelease    # per-ABI release APKs (R8 + resource shrink)
./gradlew test                    # JVM unit tests
./gradlew :audio:connectedCheck   # JNI lifecycle test (device/emulator required)

# regenerate the placeholder model (needs: pip install numpy onnx onnxruntime)
python3 tools/export_bandscnet.py --arch gate --out app/src/main/assets/bandscnet.onnx
```

### APK size

Release uses per-ABI splits and R8. The arm64-v8a APK is ~28 MB, dominated by
the 19 MB `libonnxruntime.so` from the full `onnxruntime-android` AAR. Hitting
the **< 20 MB** goal needs an operator-reduced custom ONNX Runtime build (only
the ops the model uses); the integration is otherwise size-optimized (x86 and
the unused `libonnxruntime4j_jni.so` are excluded).

### Releases (CI/CD)

GitHub Actions builds and publishes signed, zipaligned APKs:
`.github/workflows/ci.yml` runs tests + a debug build on every push/PR, and
`.github/workflows/release.yml` publishes a GitHub Release with per-ABI signed
APKs when a `v*` tag is pushed. Signing keys are supplied via repository
secrets — see [docs/release.md](docs/release.md).

## Tests

| What | Where | Runs without Android SDK? |
| ---- | ----- | ------------------------- |
| Ring buffer (+ThreadSanitizer), PCM conversion, FFT, STFT overlap-add, spectrogram processor | `native-tests/` | **Yes** — `./native-tests/run_tests.sh` |
| Android C++ syntax/type check | `native-tests/hostcheck/` | **Yes** — `./native-tests/hostcheck/syntax_check.sh` |
| State machine, buffer-size policy, capture-health monitor | `*/src/test/` | With SDK — `./gradlew test` |
| JNI create/destroy lifecycle | `audio/src/androidTest/` | Device/emulator only |

`native-tests/` is a standalone host CMake project (25 tests) that builds the
Android-independent real-time/DSP code: the ring buffer under a two-thread,
two-million-item ThreadSanitizer stress test, and FFT/STFT/overlap-add
reconstruction.

## Privacy & compliance

All audio is captured and processed **on-device** and is **never transmitted,
stored, or shared** — there is no network code. See [PRIVACY.md](PRIVACY.md).
The app respects apps that opt out of capture (`ALLOW_CAPTURE_BY_NONE`) and
attempts no workarounds; it declares the `mediaProjection` foreground-service
type for Play Console.

## Stack

Kotlin + NDK/C++17 · Oboe 1.10 (LowLatency/Exclusive) · ONNX Runtime 1.23
(NNAPI EP, CPU fallback) · AudioPlaybackCapture · min SDK 29, target SDK 35.
