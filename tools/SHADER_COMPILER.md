# Shader validation dependency

The compatibility EGL/GLES library uses the GLSL frontend from SwiftShader at
`ce69367855c4961f8ab06323512e1958fe21b80a` for shader syntax, declarations,
name resolution and type checks. It replaces the former substring blacklist.
Compiler errors are returned through `glGetShaderInfoLog`.

**This integrates the compiler frontend only.** It does not replace the
compatibility renderer with SwiftShader, implement general shader execution,
or establish OpenGL ES conformance or Android graphics completion. The existing
renderer still needs independent pixel-output verification. Passing a compile
test must not be reported as passing a rendering test.

The dependency is open-source, separate from TCL firmware. Its license and
copyright notices remain in the source checkout. No precompiled compiler or
private firmware is included in this repository.

## Host tests (macOS or Linux)

Run these commands on the host from the repository root. Install Clang with
C++ support and Git first. On macOS, use the Xcode command-line tools; on
Ubuntu/Debian, install `clang` and `build-essential`.

If the source directory does not already exist:

```sh
git clone https://github.com/google/swiftshader.git build/phase3/swiftshader-src
git -C build/phase3/swiftshader-src checkout --detach ce69367855c4961f8ab06323512e1958fe21b80a
```

The build verifies the exact revision and rejects tracked source modifications.
Set `SWIFTSHADER_SOURCE` to use another checkout of that same revision. It does
not fetch dependencies automatically.

```sh
python3 tests/test_shader_validation.py
PYTHONPATH=tests python3 -m unittest tests/test_android_readiness.py
```

Objects are cached under `build/shader-compiler/host`, with source/header,
compiler identity and flag checks. The test exercises the public GLES shader
API, including compilation status and error-log lengths. It is a host test;
ARM32 guest verification is separate.

## Android ARM32 library

Use the [Android NDK](https://developer.android.com/ndk/downloads), targeting
API 28 / `armv7a-linux-androideabi`. The local verification uses NDK r27b
(27.1.12297006). Extract the appropriate host package into an ignored build
folder or use an existing installation. Set the actual NDK root, for example:

```sh
export ANDROID_NDK_HOME="$PWD/build/toolchains/android-ndk-r27b"
python3 tools/build_shader_compiler.py --target android
```

The result is `build/shader-compiler/android/libGLES_software.so`. The C++
runtime and compiler are linked into that library; no extra `libc++_shared.so`
needs provisioning. The link rejects unresolved references. Build identity and
library SHA-256 are recorded beside it in `build.json`.

The Milestone 4 and 6 runners use this build helper when provisioning their
existing firmware-dependent tests. They require the NDK and pinned source;
the previous single-C-file link command is no longer sufficient. Their other
prerequisites and acceptance limits still apply.

## What the regression suite checks

It covers malformed statements, missing expressions and semicolons, undeclared
identifiers, type/stage violations, comment handling, legal identifier names,
preprocessing, function definitions and entrypoint errors. It also checks
that shaders with no output assignment can compile: lack of an output write
is not itself a syntax error. Such shaders do not establish useful rendering.
See the [GLSL ES 1.00 specification](https://registry.khronos.org/OpenGL/specs/es/2.0/GLSL_ES_Specification_1.00.pdf),
particularly sections 6 and 7, for the language contract.
