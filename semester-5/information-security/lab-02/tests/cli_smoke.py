"""Test the packaged EXE with its real config loader and localhost UNC paths."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile
import xml.etree.ElementTree as ET

package = Path(sys.argv[1]).resolve()
manifest = json.loads((package / "SHA256SUMS.json").read_text())
for name, checksum in manifest.items():
    assert hashlib.sha256((package / name).read_bytes()).hexdigest() == checksum, name
with zipfile.ZipFile(package / "sample.docx") as doc:
    assert doc.testzip() is None
    for name in ["[Content_Types].xml", "_rels/.rels", "word/document.xml"]:
        ET.fromstring(doc.read(name))
print("PASS: package SHA-256 manifest and DOCX fixture structure")

with tempfile.TemporaryDirectory(prefix="bsit-lab2-cli-") as directory:
    root = Path(directory).resolve()
    assert root.parent == Path(tempfile.gettempdir()).resolve()
    unc_root = "\\\\localhost\\" + root.drive[0] + "$" + str(root)[2:]
    if not Path(unc_root).is_dir():
        print("SKIP: localhost administrative share is unavailable in this account")
        sys.exit(0)
    source, output = root / "исходники", root / "backup"
    source.mkdir()
    output.mkdir()
    expected = set()
    for relative in ["", "папка1", "папка1/папка2", "папка1/папка2/папка3"]:
        folder = source / relative
        folder.mkdir(parents=True, exist_ok=True)
        for name, data in {"заметка.txt": b"hello", "report_1.docx": (package / "sample.docx").read_bytes(),
                           "report_12.docx": b"excluded", "demo.exe": (package / "demo.exe").read_bytes(),
                           "other.exe": b"excluded"}.items():
            path = folder / name
            path.write_bytes(data)
            if name != "report_12.docx" and name != "other.exe":
                expected.add(path.relative_to(source).as_posix())
    config = root / "настройки.ini"
    config.write_text(f"source={unc_root}\\исходники\narchive={unc_root}\\backup\\backup.zip\n"
                      "mask=*.txt\nmask=report_?.docx\nmask=demo.exe\n", encoding="utf-8-sig")
    result = subprocess.run([str(package / "backup_service.exe"), "once", str(config)],
                            capture_output=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    with zipfile.ZipFile(output / "backup.zip") as archive:
        assert archive.testzip() is None
        assert set(archive.namelist()) == expected and len(expected) == 12
        for name in expected:
            assert archive.read(name) == (source / name).read_bytes()
    print("PASS: packaged EXE + real UTF-8 config + localhost UNC/SMB + 12 expected files")
    result = subprocess.run([str(package / "backup_service.exe")], capture_output=True, timeout=10)
    assert result.returncode == 1 and b"1063" in result.stderr, (result.stdout, result.stderr)
    print("PASS: service-only mode reports 1063 when called outside SCM")
