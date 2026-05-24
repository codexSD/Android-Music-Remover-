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

| Phase | Scope | Gate |
| ----- | ----- | ---- |
| **1 — Capture pipeline** *(this PR)* | capture → ring buffer → Oboe passthrough | passthrough latency < 150 ms |
| 2 — Model integration | Band-SCNet ONNX behind the `AudioProcessor` seam | inference < 42 ms / 85 ms chunk |
| 3 — Stability & edge cases | routing, lifecycle, capture-blocked detection | no allocations in the audio callback |
| 4 — Release prep | Play Console compliance, size budget, opt-in telemetry | internal testing track |

## Architecture

```
MediaProjection consent ─▶ CaptureService (foreground, mediaProjection type)
                               │ builds AudioRecord (PCM float, 48 kHz, stereo)
                               ▼
        ┌───────────── native AudioEngine (JNI) ─────────────┐
        │  AudioRecordReader ─▶ SpscRingBuffer ─▶ OutputStream │
        │   (capture thread)     (lock-free)     (Oboe cb)     │
        │                                          │           │
        │                                   AudioProcessor      │
        │                          (Passthrough → Band-SCNet)   │
        └──────────────────────────────────────────────────────┘
```

- **`:app`** — Kotlin UI/service: `MainActivity`, `CaptureService`, the
  `CaptureStateMachine`.
- **`:audio`** — the native engine + its `AudioEngine` JNI wrapper. The
  `AudioProcessor` interface is the swap point for Phase 2's separator; Phase 1
  ships `PassthroughProcessor`.

## Building

Requires the Android SDK (API 35), NDK r26+, and CMake 3.22+. Point Gradle at
the SDK via `local.properties` (`sdk.dir=...`) or `$ANDROID_HOME`, then:

```bash
./gradlew :app:assembleDebug      # build the APK
./gradlew test                    # JVM unit tests (state machine, format spec)
./gradlew :audio:connectedCheck   # JNI lifecycle test (device/emulator required)
```

## Tests

| What | Where | Runs without Android SDK? |
| ---- | ----- | ------------------------- |
| Lock-free ring buffer (incl. ThreadSanitizer) + PCM conversion | `native-tests/` | **Yes** — `./native-tests/run_tests.sh` |
| Android C++ syntax/type check | `native-tests/hostcheck/` | **Yes** — `./native-tests/hostcheck/syntax_check.sh` |
| Capture state machine, buffer-size policy | `*/src/test/` | With SDK — `./gradlew test` |
| JNI create/destroy lifecycle | `audio/src/androidTest/` | Device/emulator only |

`native-tests/` is a standalone host CMake project that builds the
Android-independent real-time primitives and exercises the ring buffer under a
two-thread, two-million-item stress test with ThreadSanitizer.

## Stack

Kotlin + NDK/C++17 · Oboe 1.10 (LowLatency/Exclusive) · ONNX Runtime 1.23
(Phase 2) · AudioPlaybackCapture · min SDK 29, target SDK 35.
