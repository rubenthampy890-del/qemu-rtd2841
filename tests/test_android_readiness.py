"""Regression tests for false-positive Android framework acceptance."""

import subprocess
import unittest

from android_readiness import framework_ready


def result(stdout, rc=0):
    return subprocess.CompletedProcess([], rc, stdout=stdout, stderr="")


def services(*names):
    return result("\n".join(f"{i}\t{name}: [interface]"
                            for i, name in enumerate(names)))


class AndroidReadinessTests(unittest.TestCase):
    def setUp(self):
        self.boot = result("1\n")
        self.hwsm = result("true\n")
        self.services = services("activity", "window", "package",
                                 "SurfaceFlinger", "installd")

    def test_complete_observation(self):
        self.assertTrue(framework_ready(self.boot, self.hwsm, self.services))

    def test_previous_false_positive(self):
        self.assertFalse(framework_ready(result(""), self.hwsm,
                                        services("SurfaceFlinger", "installd")))

    def test_boot_completion_required(self):
        for value in ("", "0", "true", "11"):
            with self.subTest(value=value):
                self.assertFalse(framework_ready(result(value), self.hwsm,
                                                 self.services))

    def test_framework_services_required(self):
        for name in ("activity", "window", "package"):
            with self.subTest(name=name):
                incomplete = result(self.services.stdout.replace(
                    f"{name}:", f"{name}.unrelated:"))
                self.assertFalse(framework_ready(self.boot, self.hwsm, incomplete))

    def test_failed_commands_cannot_pass_with_success_text(self):
        for index in range(3):
            probes = [self.boot, self.hwsm, self.services]
            probes[index] = result(probes[index].stdout, 1)
            with self.subTest(index=index):
                self.assertFalse(framework_ready(*probes))


if __name__ == "__main__":
    unittest.main()
