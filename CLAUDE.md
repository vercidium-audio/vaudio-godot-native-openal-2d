# Overview

This repo is a native Godot GDExtension (C++) plugin wrapping the vaudio 2D C SDK, with OpenAL Soft as the audio backend, for non-Mono ("Native Godot") projects.

This plugin links against the packaged vaudionative shared library (vaudionative.dll / libvaudionative.so / libvaudionative.dylib), not the source. Windows, Linux and macOS are supported.

# How to use

To build on Windows, run build.bat. To build on Linux/macOS, run build-unix.sh (auto-detects the host: Linux builds an x86_64 .so, macOS builds an arm64 .dylib).

The vendored binaries under `thirdparty/` are per-platform and git-ignored - you supply them yourself:
- `thirdparty/vaudio/lib/win64/` : `vaudionative.dll` + `vaudionative.lib` (+ `glfw3.dll`) (+ `vaudio-debug-window.exe`, optional, dev builds only - see below)
- `thirdparty/vaudio/lib/linux/` : `libvaudionative.so` (+ `vaudio-debug-window`, optional, dev builds only)
- `thirdparty/vaudio/lib/mac/` : `libvaudionative.dylib` (+ `vaudio-debug-window`, optional, dev builds only)
- `thirdparty/openal/lib/win64/` : `soft_oal.dll`
- `thirdparty/openal/lib/linux/` : `libopenal.so.1`
- `thirdparty/openal/lib/mac/` : `libopenal.1.dylib`

OpenAL Soft is loaded at runtime from beside the plugin binary (LoadLibrary on Windows, dlopen on Linux/macOS), so the matching OpenAL shared library is copied into `bin/` by the build and ships in the release zip. There is no OpenAL import lib - all entry points are resolved manually in `ALManager` (`src/openal/al_manager.cpp`).

macOS is Apple Silicon (arm64) only - the vendored `libopenal.1.dylib` is arm64, so the plugin is built `arch=arm64`, not universal.

`vaudio-debug-window` renders the raytracing simulation to a separate window. It only ships in vaudio's `dev` package, not `production`, so the build scripts only copy it into `bin/` if it exists. Confirm the 2D native SDK actually produces a `dev`/debug-window build before assuming parity with the 3D plugin here - see the sibling `vaudio` repo's root CLAUDE.md.

This is the 2D counterpart to `vaudio-godot-native-openal-3d`, which is the closest precedent to draw from when porting functionality. `vaudio-godot-mono-openal-2d` (the existing Mono/C# 2D plugin) is the reference for which primitive node types, world properties and behavioural quirks are 2D-specific vs shared.

# C++ code style

- Allman brace style: opening curly braces go on their own new line (for functions, classes,
  control flow, everything), not K&R/"same line" style.
- 4 spaces per indent level, not tabs.
- Don't split comments across lines, keep it all on one line

Don't use braces if it'll just be one line inside the braces, i.e:

```
// Don't do this
if (arrays.is_empty())
{
    continue;
}

// Do this
if (arrays.is_empty())
    continue;

// Except if it's an assignment
if (arrays.is_empty())
{
    arrays = xyz;
}
```

# Logging

When logging info / warning / errors, use the NAMED functions in src/va_engine_util.h:

```c
#define VA_LOG_NAMED(...) (godot::UtilityFunctions::print(VA_LOG_TAG, get_name(), ": ", __VA_ARGS__))
#define VA_WARN_NAMED(...) (godot::UtilityFunctions::push_warning(VA_LOG_TAG, get_name(), ": ", __VA_ARGS__))
#define VA_ERROR_NAMED(...) (godot::UtilityFunctions::push_error(VA_LOG_TAG, get_name(), ": ", __VA_ARGS__))
```

If the code is not inside a Node class, use the non-NAMED versions:

```c
#define VA_LOG(...) (godot::UtilityFunctions::print(VA_LOG_TAG, __VA_ARGS__))
#define VA_WARN(...) (godot::UtilityFunctions::push_warning(VA_LOG_TAG, __VA_ARGS__))
#define VA_ERROR(...) (godot::UtilityFunctions::push_error(VA_LOG_TAG, __VA_ARGS__))
```

# References

vaudio.h lives at `thirdparty/vaudio/include/vaudio.h`

See user_context.md for more info (may not exist, this file is git-ignored)
