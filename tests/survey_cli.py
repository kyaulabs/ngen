"""Play and reload the first-party survey from a clean workspace, one command per process."""

import subprocess
import sys
import tempfile


with tempfile.TemporaryDirectory(prefix='ngen-survey-') as folder:
    base = [sys.argv[1], folder]

    def call(*args, code=0):
        result = subprocess.run([*base, *map(str, args)], capture_output=True, text=True, timeout=8, check=False)
        if result.returncode != code:
            raise AssertionError((args, result.returncode, result.stdout, result.stderr))
        return result.stdout + result.stderr

    def require(condition, detail):
        if not condition:
            raise AssertionError(detail)

    voyage = sys.argv[2:] == ['voyage']
    opening = call('status')
    if voyage:
        require('committed_system=protected' in opening, opening)
    require('Protected space' in opening and 'police' in opening and 'self-defense' in opening, opening)
    require('revision=0' in opening, opening)
    require('wrong_stage' in call('step', 0, 'venture', code=1), 'entered lawless space before the lesson')
    actions = ['learn', 'join', 'venture', 'mine', 'lose', 'recover', 'salvage', 'sell',
               'venture', 'mine', 'defeat', 'sell']
    for revision, action in enumerate(actions):
        checkpoint = call('step', revision, action)
        require(f'revision={revision + 1} ' in checkpoint, checkpoint)
        restored = call('status')
        require(f'revision={revision + 1} ' in restored, action)
        if voyage and action in ('venture', 'sell'):
            system = 'lawless' if action == 'venture' else 'protected'
            require(f'native_arrival={system}' in checkpoint, checkpoint)
            require(f'committed_system={system}' in restored, restored)
    final = call('status')
    require('vessel=2 ' in final and 'credits=40 ' in final and 'completed=2' in final, final)
    replay = call('step', 7, 'sell')
    require('receipt=8' in replay, 'missing historical receipt')
    if voyage:
        require('native_arrival=' not in replay, 'replay repeated an already committed trip')
    require(call('status') == final, 'retry changed the committed save')
    require('conflict' in call('step', 7, 'mine', code=1), 'reused revision accepted another action')
    require(call('status') == final, 'conflict changed the committed save')
