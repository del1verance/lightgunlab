# Stages the GitHub release zip: plugin source + prebuilt Win64 editor binaries
# + Resources at the zip root's LightgunLab/, plus HookerConfigs, LICENSE,
# README. Build the plugin first; binaries are taken as-is.
#
#   powershell -File Scripts\Make-ReleaseZip.ps1 -Version 1.0.0
#
# Then, after the bench pass:
#   gh release create v<ver> "dist\LightgunLab-<ver>-UE5.8-Win64.zip#Lightgun Lab plugin <ver> (UE 5.8, Win64, source + prebuilt binaries + hooker configs)" --latest --title "..." --notes-file docs\release-notes-v<ver>.md

param([Parameter(Mandatory = $true)][string]$Version)

$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$Plugin = Join-Path $Root 'Plugins\LightgunLab'
$Stage = Join-Path $env:TEMP "lightgunlab-release-$Version"
$OutDir = Join-Path $Root 'dist'
$Zip = Join-Path $OutDir "LightgunLab-$Version-UE5.8-Win64.zip"

if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force "$Stage\LightgunLab\Binaries\Win64" | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null

Copy-Item "$Plugin\LightgunLab.uplugin" "$Stage\LightgunLab\"
Copy-Item -Recurse "$Plugin\Source" "$Stage\LightgunLab\Source"
Copy-Item -Recurse "$Plugin\Resources" "$Stage\LightgunLab\Resources"
Copy-Item "$Plugin\Binaries\Win64\UnrealEditor-LightgunLab.dll" "$Stage\LightgunLab\Binaries\Win64\"
Copy-Item "$Plugin\Binaries\Win64\UnrealEditor-LightgunLab.pdb" "$Stage\LightgunLab\Binaries\Win64\"
Copy-Item "$Plugin\Binaries\Win64\UnrealEditor.modules" "$Stage\LightgunLab\Binaries\Win64\"
Copy-Item -Recurse (Join-Path $Root 'HookerConfigs') "$Stage\HookerConfigs"
Copy-Item (Join-Path $Root 'LICENSE') "$Stage\LICENSE"
Copy-Item (Join-Path $Root 'README.md') "$Stage\README.md"

if (Test-Path $Zip) { Remove-Item -Force $Zip }
Compress-Archive -Path "$Stage\*" -DestinationPath $Zip -CompressionLevel Optimal
Remove-Item -Recurse -Force $Stage
Write-Host "Staged $Zip ($([math]::Round((Get-Item $Zip).Length / 1MB, 1)) MB)"
