#!/usr/bin/env python3
"""hil_remote.py: the wrapper hands this library's HIL profile to agentrc's arduino_remote.py at the pin and turns a
missing pin into exit 2. The staging and the rig transport are agentrc's tests; a real run is a farm run's evidence."""
import contextlib
import io
import sys
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'hil-contract' / 'scripts'))
import agentrc_pin  # noqa: E402
import hil_remote  # noqa: E402
import hil_test  # noqa: E402


class Wrapper(unittest.TestCase):
    def test_delegates_with_this_profile(self):
        remote = mock.Mock()
        remote.run.return_value = 7
        with mock.patch.object(hil_test, 'engine'), mock.patch.object(agentrc_pin, 'load', return_value=remote):
            self.assertEqual(hil_remote.main(['wait', 'x']), 7)
        remote.run.assert_called_once_with(hil_test, ['wait', 'x'])

    def test_missing_pin_is_exit_2(self):
        err = io.StringIO()
        with mock.patch.object(hil_test, 'engine', side_effect=agentrc_pin.PinError('no pin')), \
                contextlib.redirect_stderr(err):
            self.assertEqual(hil_remote.main(['wait', 'x']), 2)
        self.assertIn('no pin', err.getvalue())

    def test_remote_names_are_this_librarys(self):
        self.assertTrue((ROOT / hil_test.HIL).is_file())
        self.assertTrue((ROOT / hil_test.REMOTE_ROSTER).is_file())


if __name__ == '__main__':
    unittest.main()
