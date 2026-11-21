#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
eBPF syscall monitor using Linux tracepoints and BCC.

Monitors:
  - openat: opening files
  - execve: starting processes
  - read:   read operations
  - write:  write operations

Implementation:
  - eBPF program in C is embedded below.
  - BCC compiles and loads it into the Linux kernel.
  - Kernel tracepoints send events to user space via perf buffer.
  - Python prints a live event stream and optional aggregate counters.

Run:
  sudo ./monitor.py
  sudo ./monitor.py --summary-every 5
  sudo ./monitor.py --only openat,execve
  sudo ./monitor.py --pid 1234
"""

from __future__ import annotations

import argparse
import math
import os
import ctypes as ct
import signal
import sys
import time
from typing import Dict, Set

BPF_PROGRAM = r"""
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

#define NAME_LEN 256

enum syscall_type {
    SYS_OPENAT = 1,
    SYS_EXECVE = 2,
    SYS_READ   = 3,
    SYS_WRITE  = 4,
};

struct event_t {
    u64 ts_ns;
    u32 pid;
    u32 tgid;
    u32 uid;
    u32 type;
    s32 fd;
    u64 count;
    s64 flags;
    char comm[TASK_COMM_LEN];
    char filename[NAME_LEN];
};

FILTER_CODE

BPF_PERF_OUTPUT(events);
BPF_HASH(counts, u64, u64);

static __always_inline void bump_counter(u64 key) {
    u64 zero = 0;
    u64 *value = counts.lookup_or_init(&key, &zero);
    if (value) {
        __sync_fetch_and_add(value, 1);
    }
}

TRACEPOINT_PROBE(syscalls, sys_enter_openat) {
    struct event_t event = {};
    u64 pid_tgid = bpf_get_current_pid_tgid();
    if (!should_trace(pid_tgid, SYS_OPENAT)) return 0;

    event.ts_ns = bpf_ktime_get_ns();
    event.pid = (u32)pid_tgid;
    event.tgid = (u32)(pid_tgid >> 32);
    event.uid = (u32)bpf_get_current_uid_gid();
    event.type = SYS_OPENAT;
    event.fd = args->dfd;
    event.flags = args->flags;
    event.count = 0;

    bpf_get_current_comm(&event.comm, sizeof(event.comm));
    bpf_probe_read_user_str(&event.filename, sizeof(event.filename), (void *)args->filename);

    bump_counter(SYS_OPENAT);
    events.perf_submit(args, &event, sizeof(event));
    return 0;
}

TRACEPOINT_PROBE(syscalls, sys_enter_execve) {
    struct event_t event = {};
    u64 pid_tgid = bpf_get_current_pid_tgid();
    if (!should_trace(pid_tgid, SYS_EXECVE)) return 0;

    event.ts_ns = bpf_ktime_get_ns();
    event.pid = (u32)pid_tgid;
    event.tgid = (u32)(pid_tgid >> 32);
    event.uid = (u32)bpf_get_current_uid_gid();
    event.type = SYS_EXECVE;
    event.fd = -1;
    event.flags = 0;
    event.count = 0;

    bpf_get_current_comm(&event.comm, sizeof(event.comm));
    bpf_probe_read_user_str(&event.filename, sizeof(event.filename), (void *)args->filename);

    bump_counter(SYS_EXECVE);
    events.perf_submit(args, &event, sizeof(event));
    return 0;
}

TRACEPOINT_PROBE(syscalls, sys_enter_read) {
    struct event_t event = {};
    u64 pid_tgid = bpf_get_current_pid_tgid();
    if (!should_trace(pid_tgid, SYS_READ)) return 0;

    event.ts_ns = bpf_ktime_get_ns();
    event.pid = (u32)pid_tgid;
    event.tgid = (u32)(pid_tgid >> 32);
    event.uid = (u32)bpf_get_current_uid_gid();
    event.type = SYS_READ;
    event.fd = args->fd;
    event.count = args->count;
    event.flags = 0;

    bpf_get_current_comm(&event.comm, sizeof(event.comm));
    event.filename[0] = 0;

    bump_counter(SYS_READ);
    events.perf_submit(args, &event, sizeof(event));
    return 0;
}

TRACEPOINT_PROBE(syscalls, sys_enter_write) {
    struct event_t event = {};
    u64 pid_tgid = bpf_get_current_pid_tgid();
    if (!should_trace(pid_tgid, SYS_WRITE)) return 0;

    event.ts_ns = bpf_ktime_get_ns();
    event.pid = (u32)pid_tgid;
    event.tgid = (u32)(pid_tgid >> 32);
    event.uid = (u32)bpf_get_current_uid_gid();
    event.type = SYS_WRITE;
    event.fd = args->fd;
    event.count = args->count;
    event.flags = 0;

    bpf_get_current_comm(&event.comm, sizeof(event.comm));
    event.filename[0] = 0;

    bump_counter(SYS_WRITE);
    events.perf_submit(args, &event, sizeof(event));
    return 0;
}
"""


class Event(ct.Structure):
    _fields_ = [
        ("ts_ns", ct.c_ulonglong),
        ("pid", ct.c_uint),
        ("tgid", ct.c_uint),
        ("uid", ct.c_uint),
        ("type", ct.c_uint),
        ("fd", ct.c_int),
        ("count", ct.c_ulonglong),
        ("flags", ct.c_longlong),
        ("comm", ct.c_char * 16),
        ("filename", ct.c_char * 256),
    ]


TYPE_BY_ID: Dict[int, str] = {
    1: "openat",
    2: "execve",
    3: "read",
    4: "write",
}

ID_BY_TYPE: Dict[str, int] = {value: key for key, value in TYPE_BY_ID.items()}


def parse_args(argv=None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Monitor openat, execve, read, write syscalls using eBPF/BCC."
    )
    parser.add_argument(
        "--summary-every",
        type=float,
        default=0.0,
        help="Print aggregate syscall counters every N seconds. 0 disables periodic summary.",
    )
    parser.add_argument(
        "--only",
        default="openat,execve,read,write",
        help="Comma-separated syscall list to print: openat,execve,read,write.",
    )
    parser.add_argument(
        "--pid",
        type=int,
        default=0,
        help="Filter by process PID (TGID); 0 means all processes.",
    )
    parser.add_argument(
        "--comm",
        default="",
        help="Print events only for this process command name substring.",
    )
    parser.add_argument(
        "--no-events",
        action="store_true",
        help="Do not print individual events; useful with --summary-every.",
    )
    args = parser.parse_args(argv)
    if not math.isfinite(args.summary_every) or args.summary_every < 0:
        parser.error("--summary-every must be finite and non-negative")
    if not 0 <= args.pid <= 0xffffffff:
        parser.error("--pid must be a non-negative 32-bit process ID")
    if not args.comm.isascii() or len(args.comm) > 15 or any(not 32 <= ord(c) <= 126 for c in args.comm):
        parser.error("--comm accepts at most 15 printable ASCII characters")
    selected = {item.strip() for item in args.only.split(',') if item.strip()}
    if not selected or selected.difference(ID_BY_TYPE):
        parser.error("--only must contain openat, execve, read or write")
    return args


def decode_bytes(value: bytes) -> str:
    text = value.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
    return "".join(c if c.isprintable() else "\\x%02x" % ord(c) for c in text)


def format_event(event: Event) -> str:
    syscall = TYPE_BY_ID.get(event.type, f"unknown({event.type})")
    comm = decode_bytes(bytes(event.comm))
    filename = decode_bytes(bytes(event.filename))
    timestamp = time.strftime("%H:%M:%S")

    if syscall in ("openat", "execve"):
        extra = f"path={filename or '-'}"
        if syscall == "openat":
            extra += f" flags={event.flags}"
    else:
        extra = f"fd={event.fd} count={event.count}"

    return (
        f"{timestamp} "
        f"uid={event.uid:<5} tid={event.pid:<7} pid={event.tgid:<7} "
        f"comm={comm:<16.16} syscall={syscall:<7} {extra}"
    )


def print_summary(bpf) -> None:
    parts = []
    counts = bpf.get_table("counts")
    for syscall_name in ("openat", "execve", "read", "write"):
        key = ct.c_ulonglong(ID_BY_TYPE[syscall_name])
        try:
            value = counts[key].value
        except KeyError:
            value = 0
        parts.append(f"{syscall_name}={value}")

    print("SUMMARY " + " ".join(parts), flush=True)


def build_program(args, own_pid):
    selected = {item.strip() for item in args.only.split(',') if item.strip()}
    mask = sum(1 << ID_BY_TYPE[name] for name in selected)
    # C-код строится из чисел, пользовательская строка не подставляется как код.
    needle = args.comm.encode('ascii')
    if needle:
        positions = []
        for i in range(16 - len(needle)):
            positions.append('(' + ' && '.join(f'name[{i+j}] == {byte}' for j, byte in enumerate(needle)) + ')')
        condition = ' || '.join(positions)
        comm_filter = f'char name[16] = {{0}}; bpf_get_current_comm(name, sizeof(name)); if (!({condition})) return 0;'
    else:
        comm_filter = ''
    filter_code = f"""
static __always_inline int should_trace(u64 id, u32 type) {{
    u32 tgid = id >> 32;
    if (tgid == {own_pid}U) return 0;
    if ({args.pid}U && tgid != {args.pid}U) return 0;
    if (!({mask}U & (1U << type))) return 0;
    {comm_filter}
    return 1;
}}
"""
    return BPF_PROGRAM.replace('FILTER_CODE', filter_code)


def main() -> int:
    args = parse_args()
    try:
        from bcc import BPF
    except ImportError:
        print("BCC is not available. Install python3-bpfcc and use system Python.", file=sys.stderr)
        return 1
    if os.geteuid() != 0:
        print("Run with sudo/root on Linux.", file=sys.stderr)
        return 1
    try:
        bpf = BPF(text=build_program(args, os.getpid()))
    except Exception as exc:
        print(f"Cannot load eBPF program: {exc}", file=sys.stderr)
        print("Check kernel headers, tracing support and permissions with check_env.sh.", file=sys.stderr)
        return 1
    exiting = False
    lost = 0

    def handle_event(cpu, data, size):
        if size < ct.sizeof(Event):
            return
        event = ct.cast(data, ct.POINTER(Event)).contents
        if not args.no_events:
            print(format_event(event), flush=True)

    def handle_lost(cpu, count):
        nonlocal lost
        lost += count
        print(f"Lost {count} events on CPU {cpu}", file=sys.stderr)

    def on_signal(signum, frame):
        nonlocal exiting
        exiting = True

    old_handlers = {sig: signal.signal(sig, on_signal) for sig in (signal.SIGINT, signal.SIGTERM)}
    try:
        print("Started. Ctrl+C stops tracing. Counters include matching syscall entries, not results.")
        print("TIME     UID   TID     PID     COMM             SYSCALL DETAILS", flush=True)
        bpf['events'].open_perf_buffer(handle_event, page_cnt=64, lost_cb=handle_lost)
        last_summary = time.monotonic()
        while not exiting:
            bpf.perf_buffer_poll(timeout=250)
            now = time.monotonic()
            if args.summary_every > 0 and now - last_summary >= args.summary_every:
                print_summary(bpf)
                last_summary = now
        print_summary(bpf)
        print(f"Lost events: {lost}")
    finally:
        bpf.cleanup()
        for sig, previous in old_handlers.items():
            signal.signal(sig, previous)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
