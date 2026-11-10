"""Linux/GCC app integration gate. Logs stay in the build tree; no production test flags."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--probe', required=True)
p.add_argument('--server', required=True)
p.add_argument('--mode', choices=('drone', 'client-server', 'peer-to-peer'), required=True)
a = p.parse_args()
subprocess.run(['ip', 'link', 'set', 'dev', 'lo', 'up'], check=True)
out = Path('app-smoke') / a.mode
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy')
env.pop('SMOKE_DROP_EVENTS', None)
results = []
def require(condition, detail):
    if not condition:
        raise AssertionError(detail)

for case in (range(3) if a.mode == 'drone' else (False, True)):
    children, handles = [], []
    def start(args, label, extra=None, stdin=None):
        path = out / f'{case}-{label}.log'
        f = path.open('w')
        handles.append(f)
        child = subprocess.Popen(args, env=dict(env, **(extra or {})), stdin=stdin,
                                 stdout=f, stderr=subprocess.STDOUT)
        children.append(child)
        return child, path
    try:
        if a.mode == 'drone':
            child, path = start([a.probe], 'drone', {'SMOKE_ROLE': 'drone'})
            require(child.wait(timeout=60) == 0, path)
            text = path.read_text()
            require('SMOKE_DRONE hit hidden respawn' in text and '[error]' not in text, path)
            results.append({'run': case, 'drone_death_respawn': True})
        else:
            with socket.socket() as s:
                s.bind(('127.0.0.1', 0))
                port = str(s.getsockname()[1])
            server, sp = start([a.server, '--mode', a.mode, '--bind', 'tcp://127.0.0.1', '--port', port], 'server')
            time.sleep(0.5)
            args = [a.probe, '--mode', a.mode, '--server-host', '127.0.0.1', '--server-port', port,
                    '--advertise-host', '127.0.0.1']
            extra = {'SMOKE_ROLE': 'observer'}
            if case:
                extra['SMOKE_DROP_EVENTS'] = '1'
            observer, op = start(args, 'observer', extra, subprocess.PIPE)
            deadline = time.monotonic() + 35
            while 'SMOKE_REMOTE' not in op.read_text() and time.monotonic() < deadline:
                require(observer.poll() is None, op)
                time.sleep(0.05)
            require('SMOKE_REMOTE' in op.read_text(), op)
            actor, ap = start(args, 'actor', {'SMOKE_ROLE': 'actor'})
            require(actor.wait(timeout=60) == 0, ap)
            observer.communicate(input=b'quit\n', timeout=15)
            require(observer.returncode == 0, op)
            server.send_signal(signal.SIGINT)
            require(server.wait(timeout=10) == 0, sp)
            actor_text, text = ap.read_text(), op.read_text()
            require('SMOKE_REPLAY natural manual' in actor_text, ap)
            require('[error]' not in actor_text + text + sp.read_text(), out)
            counts = [int(x) for x in re.findall(r'SMOKE_REMOTE (\d+)', text)]
            target = iter((0, 1, 0, 1))
            expected = next(target, None)
            for count in counts:
                if count == expected:
                    expected = next(target, None)
            require(expected is None, (op, counts))
            notices = re.findall(r'SMOKE_NOTICE STATUS: PLAYER \d+ (DESTROYED|SPAWNED)', text)
            require(not notices if case else 'DESTROYED' in notices and 'SPAWNED' in notices, (op, notices))
            if case:
                require('SMOKE_DROPPED' in text, op)
            results.append({'drop_events': case, 'remote_visibility': counts, 'notices': notices})
    finally:
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
        for f in handles:
            f.close()
(out / 'results.json').write_text(json.dumps(results, indent=2))
print(json.dumps(results, indent=2))
