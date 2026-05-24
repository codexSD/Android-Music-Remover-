# Privacy Policy — Vocal Remover

_Last updated: 2026-05-24_

## Summary

Vocal Remover processes audio **entirely on your device**. It does **not**
collect, store, transmit, or share any audio or personal data. The app contains
no networking code and no analytics or advertising SDKs.

## What the app does

- With your explicit consent (the system screen-capture dialog), the app
  captures audio output from other apps using Android's `AudioPlaybackCapture`
  API.
- Captured audio is processed in real time, in memory, to attenuate
  instrumental content, and is played back immediately.
- Audio exists only transiently in memory while a capture session is active. It
  is never written to disk and never leaves the device.

## What the app does not do

- It does not record audio to storage.
- It does not send audio or any data over the network.
- It does not collect identifiers, usage analytics, or crash telemetry.
- It does not capture from apps that opt out of capture
  (`ALLOW_CAPTURE_BY_NONE`), and makes no attempt to bypass that protection.
  DRM-protected content, voice calls, and apps such as Spotify are not
  capturable and are not captured.

## Permissions

- **RECORD_AUDIO** — required by the platform to receive the captured playback
  stream. No microphone audio is targeted or retained.
- **FOREGROUND_SERVICE / FOREGROUND_SERVICE_MEDIA_PROJECTION** — to keep the
  capture session running with a visible, dismissible notification.
- **POST_NOTIFICATIONS** — to show that ongoing-capture notification.

## Contact

For questions about this policy, open an issue in the project repository.
