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

CONFIG = 'test/hil/agentrc.json'
CONSUMER = '.claude/skills/hil-contract/scripts/agentrc_pin.py'
CLEAN_ENV = {k: v for k, v in os.environ.items()
             if not k.startswith('GIT_') and k not in ('AGENTRC_DIR', 'CI', 'GITHUB_ACTIONS')}


class Config(unittest.TestCase):
    def test_a_plain_library_on_profile_api_1(self):
        data = agentrc_pin.config()
        self.assertEqual({k: data[k] for k in ('cache_namespace', 'profile_api', 'features', 'extra_engines')},
                         {'cache_namespace': 'adafruit-spiflash', 'profile_api': 1, 'features': [],
                          'extra_engines': []})

    def test_the_bootstrap_and_its_config_are_contract_files(self):
        self.assertLessEqual({CONSUMER, CONFIG}, set(hil_test.BUILD.CONTRACT_FILES))


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

    def test_a_staged_tree_loads_every_script_from_its_own_copy(self):
        # a fresh interpreter: the staged modules must never replace the engines the other tests load
        helper = agentrc_pin._selection()[0]
        ident = agentrc_pin.identity()
        with tempfile.TemporaryDirectory() as d:
            tree = Path(d).resolve()
            for rel in (CONSUMER, CONFIG):
                (tree / rel).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / rel, tree / rel)
            helper.stage(ident, tree, name='agentrc')
            (tree / 'manifest.json').write_text(json.dumps({'agentrc': {'commit': ident['commit'],
                                                                        'dirty': ident['dirty']}}))
            code = '''if True:
                import json, os, sys, sysconfig
                sys.path.insert(0, sys.argv[1])
                import agentrc_pin as pin
                before = set(sys.modules)
                ident = pin.identity()
                engines = [pin.load(e).__file__ for e in (*pin.ENGINES, *pin.config()['extra_engines'])]
                paths = {k: os.path.join(v, '') for k, v in sysconfig.get_paths().items()}
                lib = (paths['stdlib'], paths['platstdlib'])
                site = (paths['purelib'], paths['platlib'])  # may sit inside stdlib
                modules = sorted(f for n in set(sys.modules) - before
                                 if (f := getattr(sys.modules[n], '__file__', None))
                                 and (f.startswith(site) or not f.startswith(lib)))
                print(json.dumps({'source': ident['source'], 'commit': ident['commit'], 'files': ident['files'],
                                  'sha256': ident['sha256'], 'engines': engines, 'modules': modules}))
            '''
            p = subprocess.run([sys.executable, '-I', '-B', '-c', code, str((tree / CONSUMER).parent)],
                               capture_output=True, text=True,
                               env={**CLEAN_ENV, 'CI': 'true', 'AGENTRC_DIR': str(tree / 'absent')})
            self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
            got = json.loads(p.stdout)
            self.assertEqual([got[k] for k in ('source', 'commit', 'files', 'sha256')],
                             ['staged', ident['commit'], ident['files'], ident['sha256']])
            for f in got['engines'] + got['modules']:
                self.assertTrue(Path(f).resolve().is_relative_to(tree / 'agentrc'), f)


if __name__ == '__main__':
    unittest.main()
