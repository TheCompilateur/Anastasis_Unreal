"""Plusieurs preuves PIE dans UN editeur (EDITOR_QUEUE_001).

Lance par editor-batch.ps1, jamais a la main : la liste des travaux arrive par la variable
ANASTASIS_EDITOR_BATCH_JOBS (un fichier JSON : [{name, script, timeout, env}, ...]).

Chaque preuve est un script PIE ordinaire (tools/unreal/*-pie.py) qui, seul, finit par
`unreal.SystemLibrary.quit_editor()`. Ici il est execute tel quel, mais dans un `unreal`
intermediaire : son `quit_editor()` termine SON travail au lieu de fermer l'editeur, et ses
rappels de tick sont suivis pour etre retires s'il les oublie. Entre deux travaux : PIE arrete,
rappels retires, variables du travail suivant posees. A la fin seulement, l'editeur se ferme.

Lignes du log, lues par editor-batch.ps1 :
    EDITOR_BATCH_JOB_BEGIN <nom>
    EDITOR_BATCH_JOB_END <nom> reason=<quit|timeout|error> seconds=<s>
    EDITOR_BATCH_COMPLETE jobs=<n>
"""
import json
import os
import sys
import time
import types

import unreal

# utf-8-sig : Set-Content -Encoding UTF8 de PowerShell 5.1 ecrit un BOM, que json refuse en utf-8.
with open(os.environ['ANASTASIS_EDITOR_BATCH_JOBS'], encoding='utf-8-sig') as _f:
    JOBS = json.loads(_f.read())
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

_real_register = unreal.register_slate_post_tick_callback
_real_unregister = unreal.unregister_slate_post_tick_callback
_real_quit = unreal.SystemLibrary.quit_editor

state = {'i': -1, 'handles': [], 'done': None, 'deadline': 0.0, 'started': 0.0, 'ending': False}
t0 = time.monotonic()


def _job_done(reason):
    if state['done'] is None:
        state['done'] = reason


class _SystemLibrary(object):
    """unreal.SystemLibrary, sauf quit_editor : la fin du travail, pas de l'editeur."""

    def __getattr__(self, name):
        return getattr(unreal.SystemLibrary, name)

    @staticmethod
    def quit_editor(*args, **kwargs):
        _job_done('quit')


class _Unreal(types.ModuleType):
    def __getattr__(self, name):
        return getattr(unreal, name)


def _register(callback):
    handle = _real_register(callback)
    state['handles'].append(handle)
    return handle


def _unregister(handle):
    if handle in state['handles']:
        state['handles'].remove(handle)
    try:
        _real_unregister(handle)
    except Exception:
        pass  # deja retire par le script


proxy = _Unreal('unreal')
proxy.SystemLibrary = _SystemLibrary()
proxy.register_slate_post_tick_callback = _register
proxy.unregister_slate_post_tick_callback = _unregister


def _start(i):
    job = JOBS[i]
    for key, value in (job.get('env') or {}).items():
        os.environ[key] = value
    state.update(i=i, handles=[], done=None, ending=False,
                 started=time.monotonic(), deadline=time.monotonic() + float(job.get('timeout', 900)))
    unreal.log('EDITOR_BATCH_JOB_BEGIN %s' % job['name'])
    namespace = {'__name__': '__main__', '__file__': job['script']}
    saved = sys.modules.get('unreal')
    sys.modules['unreal'] = proxy
    try:
        with open(job['script'], encoding='utf-8') as f:
            code = compile(f.read(), job['script'], 'exec')
        exec(code, namespace)
    except Exception as e:
        unreal.log_error('EDITOR_BATCH_JOB_ERROR %s %r' % (job['name'], e))
        _job_done('error')
    finally:
        sys.modules['unreal'] = saved


def _supervise(dt):
    now = time.monotonic()
    if state['i'] < 0:
        if now - t0 > 2.0:
            unreal.log('EDITOR_BATCH_START jobs=%d' % len(JOBS))
            _start(0)
        return
    job = JOBS[state['i']]
    if state['done'] is None and now > state['deadline']:
        unreal.log_error('EDITOR_BATCH_JOB_TIMEOUT %s after=%.1f' % (job['name'], now - state['started']))
        _job_done('timeout')
    if state['done'] is None:
        return
    # Le travail est fini (ou abandonne) : ses rappels retires, puis PIE arrete, avant le suivant.
    for handle in list(state['handles']):
        _unregister(handle)
    if les.is_in_play_in_editor():
        if not state['ending']:
            les.editor_request_end_play()
            state['ending'] = True
        return
    unreal.log('EDITOR_BATCH_JOB_END %s reason=%s seconds=%.1f' % (job['name'], state['done'], now - state['started']))
    if state['i'] + 1 < len(JOBS):
        _start(state['i'] + 1)
        return
    unreal.log('EDITOR_BATCH_COMPLETE jobs=%d' % len(JOBS))
    _real_unregister(supervisor)
    _real_quit()


supervisor = _real_register(_supervise)
