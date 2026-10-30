# Pack offline Andong satellite MBTiles (no API key in this file).
param(
  [int]$MinZoom = 12,
  [int]$MaxZoom = 15,
  [switch]$Append,
  [string]$OutPath = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $OutPath) { $OutPath = Join-Path $root "data\andong\satellite.mbtiles" }
New-Item -ItemType Directory -Force -Path (Split-Path $OutPath) | Out-Null

$pyCands = @(
  "A:\OSGeo4W\apps\Python312\python.exe",
  "D:\OSGeo4W\apps\Python312\python.exe",
  "C:\OSGeo4W\apps\Python312\python.exe",
  "python"
)
$py = $pyCands | Where-Object { $_ -eq "python" -or (Test-Path $_) } | Select-Object -First 1
$script = Join-Path $root "scripts\pack-andong-basemap.py"
$packArgs = @($script, "--min-zoom", "$MinZoom", "--max-zoom", "$MaxZoom", "--out", $OutPath)
if ($Append) { $packArgs += "--append" }
& $py @packArgs
exit $LASTEXITCODE
