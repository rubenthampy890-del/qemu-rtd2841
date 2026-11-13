"""Unit and regression tests for GLES2 shader validation in software_egl_gles2.c."""

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
from build_shader_compiler import build

HARNESS_C = r"""
#include <stdio.h>
#include <stdlib.h>
#include "tools/software_egl_gles2.c"

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    unsigned int type = (unsigned int)atoi(argv[1]);
    const char *src = argv[2];

    EGLint a[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    EGLSurface s = eglCreatePbufferSurface((EGLDisplay)1, 0, a);
    EGLContext c = eglCreateContext((EGLDisplay)1, 0, 0, 0);
    if (!s || !c) return 3;
    if (!eglMakeCurrent((EGLDisplay)1, s, s, c)) return 4;

    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, 0);
    glCompileShader(sh);

    GLint status = -1;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &status);
    GLint log_size = -1;
    char log[1024];
    GLsizei length = -1;
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &log_size);
    glGetShaderInfoLog(sh, sizeof(log), &length, log);
    if (length < 0 || log[length] != 0) return 5;
    if (log_size != (length ? length + 1 : 0)) return 6;
    if (!status && !length) return 7;
    char tiny[5] = {0, 0, 0, 0, 'X'};
    GLsizei short_length = -1;
    glGetShaderInfoLog(sh, 4, &short_length, tiny);
    if (short_length < 0 || short_length > 3 || tiny[short_length] != 0 || tiny[4] != 'X') return 8;
    fprintf(stderr, "%s", log);
    printf("%d\n", status);
    return 0;
}
"""

class ShaderValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        archive = build("host")["archive"]
        cls.temp = tempfile.TemporaryDirectory(prefix="shader_harness_")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.bin_path = Path(cls.temp.name) / "probe"
        obj = Path(cls.temp.name) / "probe.o"
        res_cc = subprocess.run([
            "clang", "-O2", "-I", str(REPO_ROOT), "-DSTANDALONE_TEST",
            "-x", "c", "-", "-c", "-o", str(obj)
        ], input=HARNESS_C, capture_output=True, text=True, timeout=30)
        if res_cc.returncode != 0:
            raise RuntimeError(f"Failed to compile probe.o:\n{res_cc.stderr}")
        res_ld = subprocess.run([
            "clang++", str(obj), archive, "-pthread", "-o", str(cls.bin_path)
        ], capture_output=True, text=True, timeout=30)
        if res_ld.returncode != 0:
            raise RuntimeError(f"Failed to link probe binary:\n{res_ld.stderr}")

    def compile_shader(self, shader_type: int, source: str) -> int:
        proc = subprocess.run(
            [str(self.bin_path), str(shader_type), source],
            capture_output=True, text=True, check=True, timeout=10
        )
        return int(proc.stdout.strip())

    def test_original_invalid_shader_rejected(self):
        # Original invalid test shader from Audit 14 / independent probe
        status = self.compile_shader(0x8B31, "this is not shader code")
        self.assertEqual(status, 0)

        status = self.compile_shader(0x8B31, "void main(){ this is not GLSL; }")
        self.assertEqual(status, 0)

    def test_valid_shader_with_invalid_in_comment_accepted(self):
        # Valid vertex shader containing "INVALID" in line comment
        vs_line_comment = (
            "// This comment contains the word INVALID\n"
            "attribute vec4 position;\n"
            "void main() {\n"
            "    gl_Position = position;\n"
            "}\n"
        )
        self.assertEqual(self.compile_shader(0x8B31, vs_line_comment), 1)

        # Valid fragment shader containing "INVALID" in block comment
        fs_block_comment = (
            "/* Regression test: INVALID keyword inside block comment */\n"
            "precision mediump float;\n"
            "void main() {\n"
            "    gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
            "}\n"
        )
        self.assertEqual(self.compile_shader(0x8B30, fs_block_comment), 1)

    def test_invalid_shader_with_invalid_in_code_rejected(self):
        vs_bad_code = (
            "attribute vec4 position;\n"
            "void main() {\n"
            "    gl_Position = position;\n"
            "    INVALID;\n"
            "}\n"
        )
        self.assertEqual(self.compile_shader(0x8B31, vs_bad_code), 0)

    def test_missing_semicolon_rejected(self):
        vs_no_semi = "attribute vec4 pos; void main() { gl_Position = pos }"
        self.assertEqual(self.compile_shader(0x8B31, vs_no_semi), 0)

    def test_non_void_main_rejected(self):
        vs_int_main = "attribute vec4 pos; int main() { gl_Position = pos; return 0; }"
        self.assertEqual(self.compile_shader(0x8B31, vs_int_main), 0)

    def test_main_with_arguments_rejected(self):
        vs_args_main = "attribute vec4 pos; void main(int x) { gl_Position = pos; }"
        self.assertEqual(self.compile_shader(0x8B31, vs_args_main), 0)

    def test_bare_qualifier_rejected(self):
        self.assertEqual(self.compile_shader(0x8B31, "attribute; void main() { gl_Position = vec4(1.0); }"), 0)
        self.assertEqual(self.compile_shader(0x8B31, "uniform; void main() { gl_Position = vec4(1.0); }"), 0)

    def test_attribute_in_fragment_shader_rejected(self):
        fs_attr = "attribute vec4 pos; void main() { gl_FragColor = pos; }"
        self.assertEqual(self.compile_shader(0x8B30, fs_attr), 0)

    def test_missing_output_is_not_a_syntax_error(self):
        fs_no_color = "precision mediump float; void main() { float x = 1.0; }"
        self.assertEqual(self.compile_shader(0x8B30, fs_no_color), 1)

    def test_invalid_statement_grammar(self):
        for body in (
            "gl_FragColor=vec4(1.0); banana ???;",
            "float x = ; gl_FragColor=vec4(1.0);",
            "float x = 1.0 gl_FragColor=vec4(1.0);",
            "gl_FragColor=vec4(unknown_name);",
            "gl_FragColor=vec4(1.0); uniform float;",
        ):
            with self.subTest(body=body):
                self.assertEqual(self.compile_shader(
                    0x8B30, "precision mediump float; void main(){" + body + "}"), 0)

    def test_keyword_substrings_are_legal_identifiers(self):
        for kind, name, output in (
            (0x8B30, "INVALID_factor", "gl_FragColor"),
            (0x8B30, "attribute_weight", "gl_FragColor"),
            (0x8B31, "discard_value", "gl_Position"),
        ):
            with self.subTest(name=name):
                source = ("precision mediump float; void main(){float " + name +
                          "=1.0; " + output + "=vec4(" + name + ");}")
                self.assertEqual(self.compile_shader(kind, source), 1)

    def test_type_and_stage_errors(self):
        cases = [
            (0x8B30, "precision mediump float; void main(){gl_FragColor=1.0;}"),
            (0x8B31, "void main(){discard; gl_Position=vec4(1.0);}"),
            (0x8B30, "precision mediump float; void main(){gl_Position=vec4(1.0);}"),
            (0x8B30, "precision mediump float; void main(){gl_FragColor=vec4(1.0); /*"),
            (0x8B31, "void main(); void helper(){gl_Position=vec4(1.0);}"),
            (0x8B31, "void main(){gl_Position=vec4(1.0);} void main(){}"),
        ]
        for kind, source in cases:
            with self.subTest(source=source):
                self.assertEqual(self.compile_shader(kind, source), 0)

    def test_preprocessor_and_functions(self):
        source = ("#define INVALID_factor 0.5\nprecision mediump float;\n"
                  "float tint(float x){return x * 2.0;}\n"
                  "void main(){gl_FragColor=vec4(tint(INVALID_factor));}")
        self.assertEqual(self.compile_shader(0x8B30, source), 1)

    def test_valid_shaders_accepted(self):
        vs_valid = "attribute vec4 pos; void main(void) { gl_Position = pos; }"
        self.assertEqual(self.compile_shader(0x8B31, vs_valid), 1)

        fs_valid = "precision mediump float; void main() { gl_FragColor = vec4(1.0); }"
        self.assertEqual(self.compile_shader(0x8B30, fs_valid), 1)

        fs_discard = "precision mediump float; void main() { discard; }"
        self.assertEqual(self.compile_shader(0x8B30, fs_discard), 1)

if __name__ == "__main__":
    unittest.main()
