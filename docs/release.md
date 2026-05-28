# Release pipeline

Signed, zipaligned APKs are built and published by GitHub Actions, not by hand.

- **CI** (`.github/workflows/ci.yml`) runs on every push/PR: native host tests
  (incl. ThreadSanitizer), the JNI syntax check, JVM unit tests, and a debug
  build (debug APKs are uploaded as run artifacts).
- **Release** (`.github/workflows/release.yml`) runs when a `v*` tag is pushed:
  it re-runs the test gates, builds **signed + 4-byte aligned** per-ABI release
  APKs, verifies them with `apksigner`/`zipalign`, and attaches them to a
  GitHub Release.

## One-time setup: signing key

Release signing reads the keystore from environment variables (wired in
`app/build.gradle.kts`), so no secret ever lives in the repo. Create a keystore
once and keep it safe — **you must reuse the same key for every future update**,
or users can't upgrade in place.

```bash
keytool -genkeypair -v \
  -keystore release.keystore -alias vocalremover \
  -keyalg RSA -keysize 2048 -validity 10000 \
  -storepass '<STORE_PASSWORD>' -keypass '<KEY_PASSWORD>' \
  -dname "CN=Vocal Remover, O=codexSD, C=US"
```

Then add these **repository secrets** (Settings → Secrets and variables →
Actions):

| Secret | Value |
| ------ | ----- |
| `KEYSTORE_BASE64` | `base64 -w0 release.keystore` (the keystore, base64-encoded) |
| `KEYSTORE_PASSWORD` | the `-storepass` value |
| `KEY_ALIAS` | `vocalremover` (the `-alias` value) |
| `KEY_PASSWORD` | the `-keypass` value |

## Cutting a release

```bash
# bump versionCode/versionName in app/build.gradle.kts, commit, then:
git tag v0.1.0
git push origin v0.1.0
```

The Release workflow does the rest and publishes `app-<abi>-release.apk` for
`arm64-v8a`, `armeabi-v7a`, and `x86_64`.

## Local signed build (optional)

```bash
export KEYSTORE_FILE=$PWD/release.keystore \
       KEYSTORE_PASSWORD='<STORE_PASSWORD>' \
       KEY_ALIAS=vocalremover KEY_PASSWORD='<KEY_PASSWORD>'
./gradlew :app:assembleRelease
# outputs: app/build/outputs/apk/release/app-<abi>-release.apk
```

When `KEYSTORE_FILE` is unset, the release build is left unsigned (debug builds
are unaffected).
