#!/usr/bin/env bash
set -euo pipefail

work_dir=$(mktemp -d)
trap 'rm -rf -- "$work_dir"' EXIT
export EBPF_DEMO_DIR="$work_dir"
echo "hello from eBPF demo" > "$work_dir/shell.txt"
cat "$work_dir/shell.txt" > /dev/null
python3 - <<'PY'
import os
from pathlib import Path
p = Path(os.environ['EBPF_DEMO_DIR']) / 'python.txt'
p.write_text('python write event\n', encoding='utf-8')
assert p.read_text(encoding='utf-8') == 'python write event\n'
print('openat/read/write/execve workload finished')
PY
