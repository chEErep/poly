#!/usr/bin/env bash
set -euo pipefail

if [[ $# -gt 1 || ( $# -eq 1 && "$1" != "--install" ) ]]; then
    echo "Usage: $0 [--install]" >&2
    exit 2
fi
if [[ ${1:-} == --install ]]; then
    sudo apt update
    sudo apt install -y python3 python3-bpfcc bpfcc-tools clang llvm "linux-headers-$(uname -r)"
fi

echo "Kernel: $(uname -r)"
echo "BCC:"
python3 -c 'from bcc import BPF; print("import OK")' || echo "Install python3-bpfcc and use system Python."
if [[ -d "/lib/modules/$(uname -r)/build" ]]; then
    echo "Kernel headers: found"
else
    echo "Kernel headers: not found for the running kernel"
fi
tracefs=/sys/kernel/tracing
[[ -d "$tracefs/events/syscalls" ]] || tracefs=/sys/kernel/debug/tracing
for syscall in openat execve read write; do
    if [[ -r "$tracefs/events/syscalls/sys_enter_$syscall/format" ]]; then
        echo "$syscall tracepoint: found"
    else
        echo "$syscall tracepoint: unavailable (check tracefs and permissions)"
    fi
done
