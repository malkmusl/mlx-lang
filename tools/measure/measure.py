#!/usr/bin/env python3
"""Measures the programs of this repository the same way each, to see
where their time and memory go and to compare before and after a change:

  - the desktop programs (the dock, the top bar, the launcher, the file
    manager, settings, the terminal, MLX Observatory) run in the nested
    compositor under tools/wayland-test-host, drawing on the GPU (lavapipe)
    and on the CPU (MLX_CANVAS=cpu), through one scenario: they start, the
    pointer sweeps over the screen, the wheel scrolls, keys are typed, then
    they close. MLX_FRAMES has std.diagnostics write their frame timeline,
    MLX_LEAK_CHECK has std.leaks write their memory: the time from the
    process's start to its first frame, the frames (the processor's and the
    GPU's time on each), what they hold at the end;
  - the command line programs (the compiler on a large program, the
    codemap on this repository, mlx-sh on a loop) for their wall time,
    processor time and most memory resident.

    tools/measure/measure.py [--compiler PATH] [--out DIR] [--only NAME,...] [--canvas gpu,cpu] [--profile] [--profile-after MS]

With --profile each desktop program runs under mlx-profile (tools/profile)
and the functions where its main thread spent the most time are listed
under the table; with --profile-after only from that many milliseconds
after its start on (its frames, not its start).

The longest pause is the longest time between the end of one frame and
the start of the next: where a program did something else than drawing
for long (MLX Observatory reading the code base after its first frame).

It prints a table (Markdown) and writes it with the numbers as JSON into
the output directory (measure.md, measure.json), so two runs can be laid
side by side. The programs are built with the compiler first."""
import argparse, concurrent.futures, json, os, resource, shutil, statistics, subprocess, sys, tempfile, time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LAVAPIPE = '/usr/share/vulkan/icd.d/lvp_icd.json'

# name: (source, how the compositor starts it, extra arguments)
DESKTOP = {
    'dock': ('projects/desktop/dock/main.mlx', 'dock', []),
    'topbar': ('projects/desktop/topbar/main.mlx', 'topbar', []),
    'launcher': ('projects/desktop/launcher/main.mlx', 'launcher', []),
    'files': ('projects/desktop/files/main.mlx', 'run', []),
    'settings': ('projects/desktop/settings/main.mlx', 'run', []),
    'terminal': ('projects/desktop/terminal/main.mlx', 'run', []),
    'observatory': ('projects/observatory/main.mlx', 'run', [REPO]),
}
BINARY = {
    'dock': 'mlx-dock', 'topbar': 'mlx-topbar', 'launcher': 'mlx-launcher', 'files': 'mlx-files',
    'settings': 'mlx-settings', 'terminal': 'mlx-terminal', 'observatory': 'mlx-observatory',
}
TOOLS = {
    'mlx-compositor': 'projects/desktop/compositor/main.mlx',
    'test-host': 'tools/wayland-test-host/main.mlx',
    'mlx-codemap': 'tools/codemap/main.mlx',
    'mlx-sh': 'projects/shell/main.mlx',
    'mlx-profile': 'tools/profile/main.mlx',
}


def build(compiler, work, names):
    sources = {}
    for name in names:
        if name in DESKTOP:
            sources[BINARY[name]] = DESKTOP[name][0]
    sources.update(TOOLS)
    def one(item):
        binary, source = item
        started = time.monotonic()
        result = subprocess.run([compiler, '--quiet', source, '-o', os.path.join(work, binary)], cwd=REPO, capture_output=True, text=True)
        if result.returncode != 0:
            sys.exit('building %s failed:\n%s%s' % (source, result.stdout, result.stderr))
        return binary, time.monotonic() - started
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        return dict(pool.map(one, sorted(sources.items())))


def scenario(width, height, launcher):
    """The test host's script: the pointer sweeps the screen in a grid, the
    wheel scrolls, keys are typed, then the window closes."""
    lines = ['wait 3000']
    if launcher:
        lines += ['down 125', 'up 125', 'wait 1500']
    for row in range(8):
        y = 40 + row * (height - 80) // 7
        for column in range(12):
            x = 40 + column * (width - 80) // 11
            lines += ['pointer %d %d' % (x, y), 'wait 40']
    lines += ['pointer %d %d' % (width // 2, height // 2)]
    for _ in range(6):
        lines += ['scroll 15', 'wait 60']
    for _ in range(6):
        lines += ['scroll -15', 'wait 60']
    lines += ['type mlx', 'wait 300']
    for _ in range(3):
        lines += ['down 14', 'up 14', 'wait 60']
    lines += ['wait 2000', 'close']
    return '\n'.join(lines) + '\n'


def read_frames(path):
    header = None
    frames = []
    for line in open(path):
        fields = line.rstrip('\n').split('\t')
        if fields[0] == 'frames':
            header = fields
        elif fields[0] == 'frame' and len(fields) >= 5:
            frames.append((int(fields[3]), int(fields[4]), int(fields[2])))
    if not header or not frames:
        return None
    pauses = [frames[at + 1][2] - frames[at][2] - frames[at][0] for at in range(len(frames) - 1)]
    def percentile(values, share):
        ordered = sorted(values)
        return ordered[min(len(ordered) - 1, len(ordered) * share // 100)]
    work = [max(processor, gpu) for processor, gpu, start in frames]
    return {
        'frames': int(header[4]),
        'startup_ms': int(header[5]) / 1000,
        'work_median_ms': percentile(work, 50) / 1000,
        'work_p95_ms': percentile(work, 95) / 1000,
        'processor_median_ms': percentile([f[0] for f in frames], 50) / 1000,
        'gpu_median_ms': percentile([f[1] for f in frames], 50) / 1000,
        'first_frame_ms': work[0] / 1000,
        'longest_pause_ms': max(pauses) / 1000 if pauses else 0,
    }


def read_memory(path):
    result = {'gpu_bytes': 0, 'gpu_blocks': 0}
    for line in open(path):
        fields = line.rstrip('\n').split('\t')
        if fields[0] == 'memory' and len(fields) >= 12:
            # program, pid, seconds, allocations, frees, blocks, bytes, mapped, peak, values, resident
            result.update(blocks=int(fields[6]), bytes=int(fields[7]), mapped=int(fields[8]), peak=int(fields[9]), values=int(fields[10]), resident_kib=int(fields[11]))
        elif fields[0] == 'gpu-block' and len(fields) >= 4:
            result['gpu_bytes'] += int(fields[3])
            result['gpu_blocks'] += 1
    return result


def read_profile(folder):
    """The processor's profile mlx-profile wrote: the functions by their own
    share, most first. mlx-profile writes it once the program has ended,
    which can be after the compositor has: it is waited for a while."""
    files = []
    for _ in range(100):
        files = [f for f in os.listdir(folder) if f.endswith('.profile') and '-gpu-' not in f] if os.path.isdir(folder) else []
        if files:
            break
        time.sleep(0.1)
    if not files:
        return []
    rows = []
    samples = 0
    for line in open(os.path.join(folder, sorted(files)[-1])):
        fields = line.rstrip('\n').split('\t')
        if fields[0] == 'profile':
            samples = int(fields[3])
        elif len(fields) >= 3 and fields[0].isdigit():
            rows.append((int(fields[0]), int(fields[1]), fields[2]))
    rows.sort(reverse=True)
    return [{'self': own, 'total': total, 'share': own * 100 / samples if samples else 0, 'function': name} for own, total, name in rows[:15]]


def run_desktop(work, name, canvas, keymap, profile=False, profile_after=0):
    source, how, extra = DESKTOP[name]
    runtime = tempfile.mkdtemp(prefix='measure-')
    os.chmod(runtime, 0o700)
    home = os.path.join(work, 'home-' + name + '-' + canvas)
    for folder in ('Documents', 'Pictures', 'Downloads', '.config', '.local/share'):
        os.makedirs(os.path.join(home, folder), exist_ok=True)
    for index in range(40):
        with open(os.path.join(home, 'Documents', 'note-%02d.txt' % index), 'w') as file:
            file.write('a note\n' * (index + 1))
    empty = os.path.join(work, 'empty')
    os.makedirs(empty, exist_ok=True)
    width, height = 1280, 800
    script = os.path.join(work, name + '-' + canvas + '.script')
    with open(script, 'w') as file:
        file.write(scenario(width, height, how == 'launcher'))
    socket = 'host-measure'
    host = subprocess.Popen([os.path.join(work, 'test-host'), socket, keymap, script], env=dict(os.environ, XDG_RUNTIME_DIR=runtime), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        if os.path.exists(os.path.join(runtime, socket)):
            break
        time.sleep(0.05)
    program = os.path.join(work, BINARY[name])
    command = [os.path.join(work, 'mlx-compositor'), '--no-fps', '--renderer', 'cpu', '--socket', 'nested-measure', '--size', '%dx%d' % (width, height), '--xwayland', 'none']
    # The program through a small wrapper script: its arguments, and
    # mlx-profile when profiling.
    profiles = os.path.join(work, 'profiles-' + name + '-' + canvas)
    wrapper = os.path.join(work, name + '-' + canvas + '.sh')
    with open(wrapper, 'w') as file:
        after = '--after %d ' % profile_after if profile_after else ''
        runner = '%s -o %s %s' % (os.path.join(work, 'mlx-profile'), profiles, after) if profile else ''
        file.write('#!/bin/sh\nexec %s%s %s\n' % (runner, program, ' '.join(extra)))
    os.chmod(wrapper, 0o755)
    for helper in ('launcher', 'dock', 'topbar'):
        command += ['--' + helper, wrapper if how == helper else 'none']
    if how == 'run':
        command += ['--run', wrapper]
    leaks = os.path.join(work, name + '-' + canvas + '.leaks')
    environment = {
        'PATH': os.environ.get('PATH', '/usr/bin:/bin'), 'HOME': home, 'XDG_RUNTIME_DIR': runtime, 'WAYLAND_DISPLAY': socket,
        'XDG_DATA_HOME': os.path.join(home, '.local/share'), 'XDG_DATA_DIRS': empty, 'XDG_CONFIG_HOME': os.path.join(home, '.config'),
        'VK_DRIVER_FILES': LAVAPIPE, 'MLX_CANVAS': canvas, 'MLX_FRAMES': '1', 'MLX_LEAK_CHECK': leaks, 'LANG': 'en_US.UTF-8', 'TZ': 'UTC',
        'SHELL': os.path.join(work, 'mlx-sh'),
    }
    log = open(os.path.join(work, name + '-' + canvas + '.log'), 'w')
    status = subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT, timeout=180).returncode
    try:
        host.wait(timeout=30)
    except subprocess.TimeoutExpired:
        host.kill()
    result = {'program': name, 'canvas': canvas, 'status': status}
    base = BINARY[name]
    for kind, reader in (('frames', read_frames), ('memory', read_memory)):
        folder = os.path.join(runtime, 'mlx', kind)
        files = [f for f in os.listdir(folder) if f.startswith(base + '-')] if os.path.isdir(folder) else []
        files.sort(key=lambda f: os.path.getmtime(os.path.join(folder, f)))
        if files:
            shutil.copy(os.path.join(folder, files[-1]), os.path.join(work, name + '-' + canvas + '.' + kind))
            values = reader(os.path.join(folder, files[-1]))
            if values:
                result.update(values)
    shutil.rmtree(runtime, ignore_errors=True)
    if profile:
        result['profile'] = read_profile(profiles)
    return result


def run_command(name, command, stdin=None, cwd=REPO):
    started = time.monotonic()
    process = subprocess.Popen(command, cwd=cwd, stdin=subprocess.PIPE if stdin else subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if stdin:
        process.stdin.write(stdin.encode())
        process.stdin.close()
    _, status, usage = os.wait4(process.pid, 0)
    return {'program': name, 'status': os.waitstatus_to_exitcode(status), 'wall_ms': (time.monotonic() - started) * 1000,
            'cpu_ms': (usage.ru_utime + usage.ru_stime) * 1000, 'max_rss_kib': usage.ru_maxrss}


def run_commands(compiler, work):
    loop = 'i=0\nwhile [ $i -lt 20000 ]; do i=$((i + 1)); x="$i:$x"; x=${x%%:*}; done\necho $i\n'
    return [
        run_command('compiler (observatory)', [compiler, '--quiet', 'projects/observatory/main.mlx', '-o', os.path.join(work, 'observatory-again')]),
        run_command('codemap stats (repository)', [os.path.join(work, 'mlx-codemap'), '-C', REPO, 'stats']),
        run_command('mlx-sh (20000 turns)', [os.path.join(work, 'mlx-sh')], stdin=loop),
    ]


def number(value, digits=1):
    if value is None:
        return '-'
    if isinstance(value, float):
        return ('%.' + str(digits) + 'f') % value
    return str(value)


def table(desktop, commands, builds):
    lines = ['## Desktop programs', '',
             '| program | canvas | start to first frame ms | longest pause ms | frames | frame work median ms | p95 ms | processor median ms | GPU median ms | resident MiB | blocks | GPU MiB |',
             '|---|---|---|---|---|---|---|---|---|---|---|---|']
    for row in desktop:
        lines.append('| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |' % (
            row['program'], row['canvas'], number(row.get('startup_ms')), number(row.get('longest_pause_ms')), number(row.get('frames')), number(row.get('work_median_ms'), 2),
            number(row.get('work_p95_ms'), 2), number(row.get('processor_median_ms'), 2), number(row.get('gpu_median_ms'), 2),
            number(row['resident_kib'] / 1024 if 'resident_kib' in row else None), number(row.get('blocks')),
            number(row['gpu_bytes'] / 1048576 if row.get('gpu_bytes') else None)))
    lines += ['', '## Command line programs', '', '| program | wall ms | processor ms | most resident MiB |', '|---|---|---|---|']
    for row in commands:
        lines.append('| %s | %s | %s | %s |' % (row['program'], number(row['wall_ms'], 0), number(row['cpu_ms'], 0), number(row['max_rss_kib'] / 1024)))
    for row in desktop:
        if row.get('profile'):
            lines += ['', '## Profile: %s (%s)' % (row['program'], row['canvas']), '', '| own % | own | with calls | function |', '|---|---|---|---|']
            for entry in row['profile']:
                lines.append('| %.1f | %d | %d | %s |' % (entry['share'], entry['self'], entry['total'], entry['function']))
    lines += ['', '## Build times (s)', '', ' '.join('%s %.1f' % (name, seconds) for name, seconds in sorted(builds.items()))]
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--compiler', default=os.path.join(REPO, 'mlx-out/bin/compiler/mlx4'))
    parser.add_argument('--out', default=None)
    parser.add_argument('--only', default=','.join(DESKTOP))
    parser.add_argument('--canvas', default='gpu,cpu')
    parser.add_argument('--no-commands', action='store_true')
    parser.add_argument('--profile', action='store_true')
    parser.add_argument('--profile-after', type=int, default=0)
    arguments = parser.parse_args()
    names = [name for name in arguments.only.split(',') if name]
    work = tempfile.mkdtemp(prefix='mlx-measure-')
    out = arguments.out or work
    os.makedirs(out, exist_ok=True)
    compiler = os.path.abspath(arguments.compiler)
    builds = build(compiler, work, names)
    keymap = os.path.join(work, 'us.xkb')
    with open(keymap, 'w') as file:
        subprocess.run(['xkbcli', 'compile-keymap', '--layout', 'us'], stdout=file, check=True)
    desktop = []
    for name in names:
        for canvas in arguments.canvas.split(','):
            row = run_desktop(work, name, canvas, keymap, arguments.profile or arguments.profile_after > 0, arguments.profile_after)
            print('measured %s (%s): %s' % (name, canvas, json.dumps(row)), file=sys.stderr)
            desktop.append(row)
    commands = [] if arguments.no_commands else run_commands(compiler, work)
    text = table(desktop, commands, builds)
    with open(os.path.join(out, 'measure.md'), 'w') as file:
        file.write(text)
    with open(os.path.join(out, 'measure.json'), 'w') as file:
        json.dump({'desktop': desktop, 'commands': commands, 'builds': builds}, file, indent=1)
    print(text)
    print('work: %s' % work, file=sys.stderr)


if __name__ == '__main__':
    main()
