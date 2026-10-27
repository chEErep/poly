param([string]$EvidenceDirectory = '', [switch]$KeepInstalled)
# Совместимое имя команды проверки; полный сценарий вынесен в соседний файл.
& (Join-Path $PSScriptRoot 'verify-complete.ps1') -EvidenceDirectory $EvidenceDirectory -KeepInstalled:$KeepInstalled
