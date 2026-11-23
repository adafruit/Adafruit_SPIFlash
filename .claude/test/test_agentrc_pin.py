#!/usr/bin/env python3
"""agentrc_pin.py, agentrc's canonical bootstrap, as this library configures and selects it: its test/hil/agentrc.json
and the agentrc it really loads (the installed one on a dev PC, the pin in CI), on a checkout and from a staged rig
tree. The bootstrap's own behaviour is agentrc's tests; nothing here reaches a rig or the network."""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / '.claude' / 'skills' / 'hil-contract' / 'scripts'))
import agentrc_pin  # noqa: E402
import hil_test  # noqa: E402

hil_test.engine()   # the selected agentrc: a missing one fails here, naming the remedy


class Config(unittest.TestCase):
    def test_a_plain_library_on_profile_api_1(self):
        data = agentrc_pin.config()
        self.assertEqual({k: data[k] for k in ('cache_namespace', 'profile_api', 'features', 'extra_engines')},
                         {'cache_namespace': 'adafruit-spiflash', 'profile_api': 1, 'features': [],
                          'extra_engines': []})
        self.assertRegex(data['commit'], r'^[0-9a-f]{40}$')


class Selected(unittest.TestCase):
    """The agentrc this checkout really selects."""

    def test_hil_test_binds_the_selected_identity(self):
        ident = agentrc_pin.identity()
        self.assertLessEqual({*agentrc_pin.ENGINES, agentrc_pin.SELF, *agentrc_pin.FILES,
                              'skills/hil/scripts/hil_remote_core.py'}, set(ident['files']))
        self.assertEqual(ident['source'], 'pin' if agentrc_pin.ci() else 'installed')
        self.assertEqual(hil_test.BUILD.AGENTRC, ident)

    def test_the_real_engines_load_from_their_staged_closure_alone(self):
        ident = agentrc_pin.identity()
        with tempfile.TemporaryDirectory() as d:
            d = Path(d).resolve()
            hil_test.BUILD.E.agentrc_source.stage(ident, d, name='agentrc')
            for f in ('.claude/skills/hil-contract/scripts/agentrc_pin.py', 'test/hil/agentrc.json'):
                (d / f).parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / f, d / f)
            (d / 'manifest.json').write_text(json.dumps({'agentrc': {'commit': ident['commit'],
                                                                     'dirty': ident['dirty']}}))
            # a fresh interpreter on the staged copy, with no install to fall back on
            code = '''if True:
                import json, sys, sysconfig
                sys.path.insert(0, sys.argv[1])
                before = set(sys.modules)
                import agentrc_pin
                ident = agentrc_pin.identity()
                engines = [agentrc_pin.load(e).__file__ for e in agentrc_pin.ENGINES]
                lib = tuple(sysconfig.get_paths()[k] for k in ('stdlib', 'platstdlib', 'purelib', 'platlib'))
                new = sorted(f for n in set(sys.modules) - before
                             if (f := getattr(sys.modules[n], '__file__', None)) and not f.startswith(lib))
                print(json.dumps({'engines': engines, 'modules': new, 'source': ident['source'],
                                  'commit': ident['commit'], 'files': ident['files'], 'sha256': ident['sha256']}))
            '''
            p = subprocess.run([sys.executable, '-I', '-B', '-c', code, str(d / '.claude/skills/hil-contract/scripts')],
                               capture_output=True, text=True, env={**os.environ, 'AGENTRC_DIR': str(d / 'absent')})
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
            got = json.loads(p.stdout)
            self.assertEqual(got['engines'], [str(d / 'agentrc' / e) for e in agentrc_pin.ENGINES])
            self.assertEqual([got[k] for k in ('source', 'commit', 'files', 'sha256')],
                             ['staged', ident['commit'], ident['files'], ident['sha256']])
            for f in got['modules']:
                self.assertTrue(Path(f).resolve().is_relative_to(d), f)


if __name__ == '__main__':
    unittest.main()
