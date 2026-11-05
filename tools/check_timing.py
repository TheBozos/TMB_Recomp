#!/usr/bin/env python3
"""Check a real-time run's menu timers and movie cadence, independently of its FPS counter.

Run with TMB_TRACE_CLOCK=1 BOZO_TIMING_LOG=1 for at least 40 seconds, then:
    python tools/check_timing.py run.log
The first second of each movie is excluded (decoder and disc startup). A sustained
speed error over five percent fails; missing observations fail instead of passing silently.
"""
import argparse
import re
from pathlib import Path

CLOCK = re.compile(r'\[tmb:clock\] frame (\d+): EE ([\d.]+) s, PC ([\d.]+) s,.*ticks (\d+) fields (\d+)')
MOVIE = re.compile(r'\[mpeg:clock\] picture (\d+) at ([\d.]+) s')


def check(text):
    clocks = [tuple(map(float, m.groups())) for m in CLOCK.finditer(text)]
    results = []
    if len(clocks) < 2:
        return ['FAIL: need at least two clock observations (TMB_TRACE_CLOCK=1)']
    first, last = clocks[0], clocks[-1]
    seconds = last[2] - first[2]
    if seconds <= 0:
        return ['FAIL: clock observations have no elapsed real time']
    for label, actual, expected in (
        ('game clock', (last[1] - first[1]) / seconds, 1),
        ('menu ticks/s', (last[3] - first[3]) / seconds, 576000),
        ('menu fields/s', (last[4] - first[4]) / seconds, 60),
    ):
        ok = abs(actual / expected - 1) <= .05
        results.append(f'{"PASS" if ok else "FAIL"}: {label}: {actual:.4f} (expected {expected})')
    movies = []
    for match in MOVIE.finditer(text):
        frame, time = int(match[1]), float(match[2])
        if frame == 0 or not movies:
            movies.append([])
        if frame >= 30:
            movies[-1].append((frame, time))
    observed = 0
    for i, movie in enumerate(movies, 1):
        if len(movie) < 2:
            continue
        span = movie[-1][1] - movie[0][1]
        fps = (movie[-1][0] - movie[0][0]) / span if span > 0 else float('inf')
        ok = abs(fps / (30000 / 1001) - 1) <= .05
        results.append(f'{"PASS" if ok else "FAIL"}: movie {i}: {fps:.4f} pictures/s (expected 29.97)')
        observed += 1
    if not observed:
        results.append('FAIL: need movie observations (BOZO_TIMING_LOG=1)')
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    results = check(args.log.read_text(encoding='utf-8', errors='replace'))
    print('\n'.join(results))
    return int(any(line.startswith('FAIL:') for line in results))


if __name__ == '__main__':
    raise SystemExit(main())
