"""Exercise the real service loop without installing a service on the host."""
import ctypes
from ctypes import wintypes
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import zipfile

probe = Path(sys.argv[1]).resolve()
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                              ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]


def wait_for(archive, expected, process, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        assert process.poll() is None, "Monitoring process exited unexpectedly"
        try:
            with zipfile.ZipFile(archive) as z:
                actual = {name: z.read(name) for name in z.namelist()}
                if actual == expected and z.testzip() is None:
                    return
        except (OSError, zipfile.BadZipFile, EOFError):
            pass
        time.sleep(0.05)
    raise AssertionError(f"Archive did not reach expected contents: {sorted(expected)}")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


with tempfile.TemporaryDirectory(prefix="bsit-lab2-watch-") as directory:
    root = Path(directory).resolve()
    assert root.parent == Path(tempfile.gettempdir()).resolve()
    source, target = root / "исходники", root / "backup"
    source.mkdir()
    target.mkdir()
    archive = target / "backup.zip"
    deep = source / "папка1/папка2/папка3"
    deep.mkdir(parents=True)
    (source / "a.txt").write_bytes(b"AAAA")
    (deep / "deep.txt").write_bytes(b"deep")
    (source / "empty.txt").touch()
    expected = {"a.txt": b"AAAA", "папка1/папка2/папка3/deep.txt": b"deep", "empty.txt": b""}
    process = subprocess.Popen([str(probe), str(source), str(archive), "--watch", "*.txt"],
                               cwd=root, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                               stderr=subprocess.PIPE)
    try:
        wait_for(archive, expected, process)
        stamp = archive.stat().st_mtime_ns
        time.sleep(0.6)
        assert archive.stat().st_mtime_ns == stamp, "Unchanged ZIP was needlessly rewritten"
        (source / "ignored.bin").write_bytes(b"excluded")
        time.sleep(0.6)
        assert archive.stat().st_mtime_ns == stamp, "Excluded file triggered an update"
        print("PASS: initial ZIP; unchanged and excluded files do not rewrite it")

        old_time = (source / "a.txt").stat()
        (source / "a.txt").write_bytes(b"BBBB")
        os.utime(source / "a.txt", ns=(old_time.st_atime_ns, old_time.st_mtime_ns))
        expected["a.txt"] = b"BBBB"
        wait_for(archive, expected, process)
        (deep / "новый.txt").write_bytes("Новый файл".encode())
        expected["папка1/папка2/папка3/новый.txt"] = "Новый файл".encode()
        wait_for(archive, expected, process)
        print("PASS: same-size edit with preserved timestamp; new Unicode file at level 3")

        previous = digest(archive)
        held = kernel.CreateFileW(str(archive), 0x80000000, 1, None, 3, 0, None)
        assert held != ctypes.c_void_p(-1).value, ctypes.get_last_error()
        try:
            (source / "a.txt").write_bytes(b"CCCC")
            expected["a.txt"] = b"CCCC"
            time.sleep(0.7)
            assert digest(archive) == previous and process.poll() is None
        finally:
            kernel.CloseHandle(held)
        wait_for(archive, expected, process)
        print("PASS: destination lock preserves old ZIP; update retries after release")

        held = kernel.CreateFileW(str(source / "a.txt"), 0x80000000, 0, None, 3, 0, None)
        assert held != ctypes.c_void_p(-1).value, ctypes.get_last_error()
        previous = digest(archive)
        try:
            time.sleep(0.5)
            assert process.poll() is None and digest(archive) == previous
        finally:
            kernel.CloseHandle(held)
        (source / "a.txt").write_bytes(b"DDDD")
        expected["a.txt"] = b"DDDD"
        wait_for(archive, expected, process)
        print("PASS: locked source does not kill monitoring; later edits are copied")

        (deep / "deep.txt").rename(deep / "renamed.txt")
        expected["папка1/папка2/папка3/renamed.txt"] = expected.pop("папка1/папка2/папка3/deep.txt")
        wait_for(archive, expected, process)
        (source / "empty.txt").unlink()
        expected.pop("empty.txt")
        wait_for(archive, expected, process)
        archive.unlink()
        wait_for(archive, expected, process)
        archive.write_bytes(b"broken ZIP")
        wait_for(archive, expected, process)
        print("PASS: rename/delete mirrored; missing or corrupt ZIP recreated at same path")

        source.rename(root / "temporarily-offline")
        previous = digest(archive)
        time.sleep(0.5)
        assert digest(archive) == previous and process.poll() is None
        (root / "temporarily-offline").rename(source)
        (source / "recovered.txt").write_bytes(b"back online")
        expected["recovered.txt"] = b"back online"
        wait_for(archive, expected, process)
        print("PASS: temporary source outage preserves ZIP and recovers automatically")
    finally:
        start = time.monotonic()
        process.communicate(b"\n", timeout=5)
        assert process.returncode == 0 and time.monotonic() - start < 3
    assert not list(target.glob("bsi*.tmp"))
    print("PASS: responsive stop and no temporary-file leftovers")
