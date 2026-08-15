# Copyright (c) 2026 del1verance. MIT License.
#
# Builds the source-only plugin zip for Fab submission.
# Fab compiles code plugins itself, so the archive must contain no
# Binaries/ or Intermediate/ - just the .uplugin, Source, and Resources.
#
# Usage:  powershell -ExecutionPolicy Bypass -File Scripts\Make-FabZip.ps1 [-Version 0.1.0]

param([string]$Version = "0.1.0")
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$Plugin = Join-Path $Root 'Plugins\LightgunLab'
$Dist = Join-Path $Root 'dist'
$Stage = Join-Path $Dist 'LightgunLab'

if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force $Stage | Out-Null

Copy-Item (Join-Path $Plugin 'LightgunLab.uplugin') $Stage
Copy-Item (Join-Path $Plugin 'Source') (Join-Path $Stage 'Source') -Recurse
Copy-Item (Join-Path $Plugin 'Resources') (Join-Path $Stage 'Resources') -Recurse

$Zip = Join-Path $Dist "LightgunLab-Fab-$Version.zip"
if (Test-Path $Zip) { Remove-Item $Zip }
Compress-Archive -Path $Stage -DestinationPath $Zip

Write-Host "Fab submission zip: $Zip"
Write-Host ("Size: {0:N2} MB" -f ((Get-Item $Zip).Length / 1MB))
Write-Host "Upload via a public, no-login link (the Fab portal fetches it)."
