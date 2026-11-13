#!/usr/bin/env python3
"""Build the pinned GLSL frontend and optional ARM32 compatibility GLES library.

No implicit downloads or installations. See tools/SHADER_COMPILER.md.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent
REVISION = "ce69367855c4961f8ab06323512e1958fe21b80a"


def run(command, **kwargs):
    return subprocess.run([str(x) for x in command], check=True, timeout=180,
                          **kwargs)


def build(target="host", source=None, ndk=None, output=None):
    source = Path(source or os.environ.get("SWIFTSHADER_SOURCE",
                  ROOT / "build/phase3/swiftshader-src")).resolve()
    if not (source / ".git").exists():
        raise RuntimeError("Missing SwiftShader source; see tools/SHADER_COMPILER.md")
    head = run(["git", "-C", source, "rev-parse", "HEAD"],
               capture_output=True, text=True).stdout.strip()
    dirty = run(["git", "-C", source, "status", "--porcelain",
                 "--untracked-files=no"], capture_output=True, text=True).stdout
    if head != REVISION or dirty:
        raise RuntimeError("SwiftShader source must be clean at " + REVISION)
    out = Path(output or ROOT / "build/shader-compiler" / target).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if target == "android":
        ndk = Path(ndk or os.environ.get("ANDROID_NDK_HOME",
                   ROOT / "build/toolchains/android-ndk-r27b")).resolve()
        host = "darwin-x86_64" if platform.system() == "Darwin" else "linux-x86_64"
        toolbin = ndk / "toolchains/llvm/prebuilt" / host / "bin"
        cc = toolbin / "armv7a-linux-androideabi28-clang"
        cxx = toolbin / "armv7a-linux-androideabi28-clang++"
        ar = toolbin / "llvm-ar"
        if not all(p.is_file() for p in (cc, cxx, ar)):
            raise RuntimeError("Android NDK tools missing; set ANDROID_NDK_HOME")
    else:
        cc, cxx, ar = (shutil.which(x) for x in ("clang", "clang++", "ar"))
        if not all((cc, cxx, ar)):
            raise RuntimeError("Host clang, clang++ and ar are required")
    compiler = source / "src/OpenGL/compiler"
    tracked = run(["git", "-C", source, "ls-files", "-z", "--",
                   "src/OpenGL/compiler"], capture_output=True).stdout
    files = sorted(source / name.decode() for name in tracked.split(b"\0")
                   if name and name.endswith(b".cpp") and
                   Path(name.decode()).name not in
                   ("OutputASM.cpp", "TranslatorASM.cpp", "ossource_win.cpp"))
    files.append(ROOT / "tools/shader_validator.cpp")
    flags = ["-std=c++14", "-O2", "-fPIC", "-fvisibility=hidden", "-DNDEBUG",
             "-DANGLE_DISABLE_TRACE", "-Wno-inconsistent-missing-override",
             "-pthread", "-I" + str(compiler), "-I" + str(source / "src"),
             "-I" + str(source / "include"),
             "-I" + str(ROOT / "tools/shader_compat")]
    identity = run([cxx, "--version"], capture_output=True, text=True).stdout
    objects = []
    # Cache key includes all compiler/common headers, tool identity and flags.
    h = hashlib.sha256((REVISION + identity + repr(flags)).encode())
    for directory in (compiler, source / "src/Common", source / "include",
                      ROOT / "tools/shader_compat"):
        for p in sorted(directory.rglob("*")):
            if p.is_file() and p.suffix in (".h", ".hpp"):
                h.update(p.read_bytes())
    for index, src in enumerate(files):
        digest = hashlib.sha256(h.digest() + src.read_bytes()).hexdigest()
        obj = out / f"{index}-{src.stem}-{digest[:16]}.o"
        if not obj.exists():
            temporary = obj.with_suffix(".tmp.o")
            run([cxx, *flags, "-c", src, "-o", temporary])
            temporary.replace(obj)
        objects.append(obj)
    archive = out / "libshader_validator.a"
    temporary_archive = out / "libshader_validator.tmp.a"
    temporary_archive.unlink(missing_ok=True)
    run([ar, "rcs", temporary_archive, *objects])
    temporary_archive.replace(archive)
    result = {"target": target, "revision": REVISION,
              "compiler": identity, "archive": str(archive)}
    if target == "android":
        obj = out / "software_egl_gles2.o"
        run([cc, "-O2", "-fPIC", "-ffreestanding", "-c",
             ROOT / "tools/software_egl_gles2.c", "-o", obj])
        lib = out / "libGLES_software.so"
        tmp = out / "libGLES_software.tmp.so"
        run([cxx, "-shared", "-static-libstdc++", "-Wl,--no-undefined",
             "-Wl,--exclude-libs,ALL", "-Wl,-soname,libGLES_software.so",
             obj, archive, "-llog", "-o", tmp])
        tmp.replace(lib)
        result.update(library=str(lib), sha256=hashlib.sha256(lib.read_bytes()).hexdigest())
    (out / "build.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("host", "android"), default="host")
    parser.add_argument("--source")
    parser.add_argument("--ndk")
    parser.add_argument("--output")
    args = parser.parse_args()
    print(json.dumps(build(args.target, args.source, args.ndk, args.output), indent=2))
