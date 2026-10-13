"""Verify release names and ensure untrusted suffixes never become shell code."""
import os
from pathlib import Path
import subprocess
import shutil
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('release-version.sh').resolve()
# Windows' PATH may resolve bash to the WSL launcher, even from a Git Bash CI step.
# Select the bash distributed with the same Git installation that runs our fixtures.
BASH = shutil.which('bash')
if os.name == 'nt':
    git = Path(shutil.which('git') or '')
    for candidate in (git.parent.parent / 'bin' / 'bash.exe', git.parent.parent / 'usr' / 'bin' / 'bash.exe'):
        if candidate.is_file():
            BASH = str(candidate)
            break
if BASH is None:
    raise RuntimeError('Git Bash or bash is required for the release version tests')

class VersionTest(unittest.TestCase):
    def version(self, event='pull_request', ref='refs/pull/18/merge', suffix='', message='ordinary change'):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'CMakeLists.txt').write_text('project(MUTAGEN VERSION 0.3.0 LANGUAGES C CXX)\n')
            subprocess.run(['git', 'init', '-q'], cwd=root, check=True)
            subprocess.run(['git', '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                            'commit', '-qm', message, '--allow-empty'], cwd=root, check=True)
            env = dict(os.environ, GITHUB_EVENT_NAME=event, GITHUB_REF=ref, RELEASE_SUFFIX=suffix, GITHUB_SHA='HEAD')
            return subprocess.run([BASH, SCRIPT.as_posix()], cwd=root, env=env, text=True, capture_output=True)

    def test_pull_request_cannot_release_by_marker(self):
        self.assertEqual(self.version(message='change [release:beta.4]').stdout.strip(), '0.3.0')
    def test_main_explicit_marker(self):
        self.assertEqual(self.version('push', 'refs/heads/main', message='merge [release:beta.4]').stdout.strip(), '0.3.0-beta.4')
    def test_normal_main(self):
        self.assertEqual(self.version('push', 'refs/heads/main').stdout.strip(), '0.3.0')
    def test_tag_is_authoritative(self):
        self.assertEqual(self.version('push', 'refs/tags/v0.3.0-beta.2', 'ignored').stdout.strip(), '0.3.0-beta.2')
    def test_manual_suffix(self):
        self.assertEqual(self.version('workflow_dispatch', 'refs/heads/main', 'alpha.1').stdout.strip(), '0.3.0-alpha.1')
    def test_shell_suffix_rejected(self):
        self.assertNotEqual(self.version(suffix='$(echo injected)').returncode, 0)
    def test_bad_marker_rejected(self):
        self.assertNotEqual(self.version('push', 'refs/heads/main', message='merge [release:beta;.4]').returncode, 0)
    def test_bad_tag_rejected(self):
        self.assertNotEqual(self.version('push', 'refs/tags/vbad').returncode, 0)

if __name__ == '__main__':
    unittest.main()
