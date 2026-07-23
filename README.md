# SDL2 Android ARM64 Builder

This repository builds a reusable SDL2 Android dependency bundle on a GitHub-hosted Ubuntu runner.

Pinned target:

- SDL2 2.32.10
- Android `arm64-v8a`
- minimum API 30
- NDK 27.0.12077973

The output contains `libSDL2.so`, SDL2 headers, and the exact matching `org/libsdl/app` Java sources.

## Run

```bash
gh workflow run build-sdl2.yml --ref main
gh run watch --compact
```

Find and download a successful run:

```bash
gh run list --workflow build-sdl2.yml --status success --limit 5
gh run download RUN_ID --name sdl2-android-arm64
```

Verify:

```bash
sha256sum -c SDL2-2.32.10-android-arm64.zip.sha256
```

Publish the verified ZIP as a GitHub Release because workflow artifacts expire.
