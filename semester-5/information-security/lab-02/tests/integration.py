"""Independent ZIP verification; this does not claim to test SCM or SMB."""
import ctypes
from ctypes import wintypes
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

probe = Path(sys.argv[1]).resolve()
demo = Path(sys.argv[2]).resolve()
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                              ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]


def run(source, archive, masks, expected=0, option=None):
    args = [str(probe), str(source), str(archive)]
    if option:
        args.append(option)
    # MinGW expands wildcard arguments against the working directory.
    result = subprocess.run(args + masks, cwd=source.parent, capture_output=True, timeout=30)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)


def digest(path):
    return hashlib.sha256(path.read_bytes()).digest()


def lock(path, share=0):
    handle = kernel.CreateFileW(str(path), 0x80000000, share, None, 3, 0, None)
    assert handle != ctypes.c_void_p(-1).value, ctypes.get_last_error()
    return handle


with tempfile.TemporaryDirectory(prefix="bsit-lab2-") as directory:
    root = Path(directory).resolve()
    # TemporaryDirectory removes only this newly created directory when done.
    assert root.parent == Path(tempfile.gettempdir()).resolve()
    source, target = root / "исходники", root / "backup"
    source.mkdir()
    target.mkdir()
    archive = target / "копия.zip"
    expected = {}
    folders = [source, source / "папка1", source / "папка1/папка2", source / "папка1/папка2/папка3"]
    for folder in folders:
        folder.mkdir(parents=True, exist_ok=True)
        files = {"заметка.txt": "Тест UTF-8\n".encode(), "report_1.docx": b"docx bytes",
                 "demo.exe": demo.read_bytes(), "report_12.docx": b"excluded",
                 "other.exe": b"excluded", "photo.png": b"excluded"}
        for name, data in files.items():
            path = folder / name
            path.write_bytes(data)
            if name in ("заметка.txt", "report_1.docx", "demo.exe"):
                expected[path.relative_to(source).as_posix()] = data
    (source / "empty.txt").touch()
    expected["empty.txt"] = b""
    (source / "large.bin").write_bytes(os.urandom(3 * 1024 * 1024))
    expected["large.bin"] = (source / "large.bin").read_bytes()
    masks = ["*.txt", "report_?.docx", "demo.exe", "*.bin"]
    run(source, archive, masks)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert set(z.namelist()) == set(expected), z.namelist()
        assert len(z.namelist()) == len(set(z.namelist()))
        for name, data in expected.items():
            assert z.read(name) == data, name
        assert z.read("demo.exe")[:2] == b"MZ"
    print("PASS: ZIP CRC, contents, masks, Unicode, empty file, 3-level nesting, EXE bytes")

    (source / "заметка.txt").write_bytes(b"changed contents")
    (source / "new.txt").write_bytes(b"new file")
    run(source, archive, masks)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert z.read("заметка.txt") == b"changed contents"
        assert z.read("new.txt") == b"new file"
        assert len(z.namelist()) == len(set(z.namelist()))
    print("PASS: explicit second snapshot replaces the same ZIP without duplicate entries")

    previous = digest(archive)
    held = lock(source / "заметка.txt")
    try:
        run(source, archive, masks, expected=1)
    finally:
        kernel.CloseHandle(held)
    assert digest(archive) == previous and not list(target.glob("bsi*.tmp"))
    print("PASS: locked source preserves the old archive and cleans the temporary file")

    held = lock(archive, share=1)
    try:
        run(source, archive, masks, expected=1)
    finally:
        kernel.CloseHandle(held)
    assert digest(archive) == previous and not list(target.glob("bsi*.tmp"))
    print("PASS: locked destination preserves the old archive")

    for option in ["--cancel", "--cancel-later"]:
        (source / "large.bin").write_bytes(os.urandom(32 * 1024 * 1024))
        run(source, archive, masks, expected=1, option=option)
        assert digest(archive) == previous and not list(target.glob("bsi*.tmp"))
    print("PASS: cancellation before and during compression preserves the old archive")

    run(root / "missing", archive, masks, expected=1)
    assert digest(archive) == previous
    run(source, target / "empty.zip", ["*.not-present"])
    with zipfile.ZipFile(target / "empty.zip") as z:
        assert not z.namelist() and z.testzip() is None
    print("PASS: missing source and a valid empty ZIP")
