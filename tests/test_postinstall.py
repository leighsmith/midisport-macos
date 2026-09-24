"""Exercise postinstall without network access, root, or installed-file changes.

Run: python3 -m unittest discover -s tests -v
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "Installer/scripts/postinstall"


class PostinstallTests(unittest.TestCase):
    def run_installer(self, failure=""):
        with tempfile.TemporaryDirectory(prefix="midisport test ") as directory:
            root = Path(directory)
            scripts = root / "scripts with spaces"
            scripts.mkdir()
            shutil.copy2(SCRIPT, scripts / "postinstall")
            helper = scripts / "extractMAudioFirmware.sh"
            helper.write_text('#!/bin/sh\necho helper >> "$TEST_TRACE"\n')
            helper.chmod(0o755)
            commands = root / "bin"
            commands.mkdir()
            trace = root / "trace"
            temporary = root / "download"
            mock = r'''#!/bin/sh
command=${0##*/}
echo "$command $*" >> "$TEST_TRACE"
case "$command" in
    mktemp) /bin/mkdir "$TEST_TEMP"; echo "$TEST_TEMP" ;;
    curl)
        case " $* " in *" --fail "*) ;; *) exit 99 ;; esac
        [ "$TEST_FAILURE" != download ] || exit 22 ;;
    tar)
        [ "$TEST_FAILURE" != archive ] || exit 1
        /bin/mkdir "$TEST_TEMP/midisport-firmware-1.2"
        for f in MidiSportLoader MidiSport1x1 MidiSport2x2 MidiSport4x4; do
            if [ "$TEST_FAILURE" = missing ] && [ "$f" = MidiSport4x4 ]; then
                continue
            fi
            echo ':00000001FF' > "$TEST_TEMP/midisport-firmware-1.2/$f.ihx"
        done ;;
    mkdir) [ "$TEST_FAILURE" != directory ] || exit 1 ;;
    cp) [ "$TEST_FAILURE" != copy ] || exit 1 ;;
esac
'''
            for name in ("mktemp", "curl", "tar", "mkdir", "cp"):
                command = commands / name
                command.write_text(mock)
                command.chmod(0o755)
            result = subprocess.run(
                ["/bin/sh", str(scripts / "postinstall")],
                env={**os.environ, "PATH": f"{commands}:/usr/bin:/bin",
                     "TEST_TRACE": str(trace), "TEST_TEMP": str(temporary),
                     "TEST_FAILURE": failure},
                capture_output=True, text=True,
            )
            actions = trace.read_text()
            self.assertFalse(temporary.exists(), "temporary download must be cleaned up")
            return result, actions

    def test_success_with_spaces_in_script_path(self):
        result, actions = self.run_installer()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Installed MIDISPORT firmware files", result.stdout)
        self.assertIn("helper\n", actions)

    def test_required_firmware_failures_are_reported(self):
        for failure in ("download", "archive", "missing", "directory", "copy"):
            with self.subTest(failure=failure):
                result, actions = self.run_installer(failure)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("Installed MIDISPORT firmware files", result.stdout)
                self.assertNotIn("helper\n", actions)
                if failure in ("download", "archive", "missing"):
                    self.assertNotIn("\ncp ", actions)


if __name__ == "__main__":
    unittest.main()
