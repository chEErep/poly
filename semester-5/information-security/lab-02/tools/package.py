"""Package the Windows service, configuration, README and integration utilities."""
from pathlib import Path
import hashlib
import json
import shutil
import sys
import zipfile

project, build = (Path(arg).resolve() for arg in sys.argv[1:3])
output = project / "dist" / "Lab2"
output.mkdir(parents=True, exist_ok=True)
files = {
    "backup_service.exe": build / "backup_service.exe",
    "demo.exe": build / "demo.exe",
    "backup.ini": project / "backup.ini",
    "README.md": project / "README.md",
    "setup-demo.ps1": project / "scripts/setup-demo.ps1",
    "verify-service.ps1": project / "scripts/verify-service.ps1",
    "verify-complete.ps1": project / "scripts/verify-complete.ps1",
    "update-demo.ps1": project / "scripts/update-demo.ps1",
    "MINIZ-LICENSE.txt": project / "third_party/miniz/LICENSE",
}
# Remove study notes left by earlier versions of the package.
for directory in (output, output.parent / "Stage1"):
    for name in ("FIRST_SHOW.md", "DEFENSE.md", "DEFENSE_SPEECH.md"):
        (directory / name).unlink(missing_ok=True)
for name, source in files.items():
    if source.suffix == ".ps1":
        # Windows PowerShell 5.1 needs a BOM for Russian literals in UTF-8 scripts.
        (output / name).write_text(source.read_text(encoding="utf-8-sig"), encoding="utf-8-sig")
    else:
        shutil.copy2(source, output / name)

# A real, minimal DOCX: it can be opened in Word, not just renamed from TXT.
with zipfile.ZipFile(output / "sample.docx", "w", zipfile.ZIP_DEFLATED) as doc:
    doc.writestr("[Content_Types].xml", '''<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/></Types>''')
    doc.writestr("_rels/.rels", '''<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>''')
    doc.writestr("word/document.xml", '''<?xml version="1.0" encoding="UTF-8"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:p><w:r><w:t>BSIT Lab 2: test document for the backup service.</w:t></w:r></w:p><w:sectPr/></w:body></w:document>''')

names = sorted([*files, "sample.docx"])
manifest = {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in names}
(output / "SHA256SUMS.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
bundle = output.parent / "Lab2.zip"
with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as archive:
    for name in [*names, "SHA256SUMS.json"]:
        archive.write(output / name, "Lab2/" + name)
print("Package: dist/Lab2 and dist/Lab2.zip")

# Старая общая папка VirtualBox продолжает открывать актуальную версию.
compatibility = output.parent / "Stage1"
compatibility.mkdir(exist_ok=True)
for name in [*names, "SHA256SUMS.json"]:
    shutil.copy2(output / name, compatibility / name)
with zipfile.ZipFile(output.parent / "Lab2-Stage1.zip", "w", zipfile.ZIP_DEFLATED) as archive:
    for name in [*names, "SHA256SUMS.json"]:
        archive.write(output / name, "Stage1/" + name)
