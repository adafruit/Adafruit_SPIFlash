#!/usr/bin/env python3
"""agentrc_pin.py, agentrc's canonical bootstrap, as this library configures and selects it: its test/hil/agentrc.json
and the agentrc it really loads (the installed one on a dev PC, the pin in CI). The bootstrap's own behaviour is
agentrc's tests, and a staged rig tree is `agentrc_pin.py test`'s conformance check; nothing here reaches a rig or the
network."""
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'hil-contract' / 'scripts'))
import agentrc_pin  # noqa: E402
import hil_test  # noqa: E402

hil_test.engine()   # the selected agentrc: a missing one fails here, naming the remedy

CONSUMER = '.claude/skills/hil-contract/scripts/agentrc_pin.py'


class Config(unittest.TestCase):
    def test_a_plain_library_on_profile_api_1(self):
        data = agentrc_pin.config()
        self.assertEqual({k: data[k] for k in ('cache_namespace', 'profile_api', 'features', 'extra_engines')},
                         {'cache_namespace': 'adafruit-spiflash', 'profile_api': 1, 'features': [],
                          'extra_engines': []})


class Selected(unittest.TestCase):
    """The agentrc this checkout really selects."""

    def test_hil_test_binds_the_selected_identity(self):
        ident = agentrc_pin.identity()
        self.assertLessEqual({*agentrc_pin.ENGINES, agentrc_pin.SELF, *agentrc_pin.FILES,
                              'skills/hil/scripts/hil_remote_core.py'}, set(ident['files']))
        self.assertEqual(ident['source'], 'pin' if agentrc_pin.ci() else 'installed')
        self.assertEqual(hil_test.BUILD.AGENTRC, ident)

    def test_this_copy_is_the_selected_canonical_bootstrap(self):
        canonical = Path(agentrc_pin.identity()['root']) / 'skills/arduino/scripts/agentrc_pin.py'
        self.assertEqual((ROOT / CONSUMER).read_bytes(), canonical.read_bytes())


if __name__ == '__main__':
    unittest.main()
