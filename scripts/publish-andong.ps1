# 안동뷰어 포터블 폴더. QGIS/Qt 런타임은 ka-hgis 포터블 것을 재활용한다.
param(
  [string]$OutDir = "",
  [string]$RuntimeDir = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$srcExe = Join-Path $root "build\Release\andong-viewer.exe"
if (-not (Test-Path $srcExe)) { throw "Build andong-viewer.exe first (Release)." }

$out = if ($OutDir) { $OutDir } else { Join-Path $root "dist\andong-portable" }
# QGIS/Qt 런타임. 안동뷰어를 ka-hgis 에서 떼어낸 뒤에도 런타임은 같은 것을 쓴다.
# -RuntimeDir 로 직접 줄 수 있고, 없으면 이 프로젝트 안 → 옆의 ka-hgis 순으로 찾는다.
$runtime = if ($RuntimeDir) { $RuntimeDir } else { Join-Path (Join-Path $root "dist") "runtime" }
if (-not (Test-Path (Join-Path $runtime "apps\qgis-dev"))) {
  $fallback = "D:\hgis\dist\ka-hgis-portable"
  if (Test-Path (Join-Path $fallback "apps\qgis-dev")) {
    $runtime = $fallback
  } else {
    throw "QGIS 런타임을 찾지 못했습니다. -RuntimeDir 로 ka-hgis 포터블 폴더를 지정하세요."
  }
}

New-Item -ItemType Directory -Force -Path $out | Out-Null
if (Test-Path $runtime) {
  robocopy $runtime $out /E /XO /NFL /NDL /NJH /NJS /nc /ns /np /XF "ka-hgis.exe" "ka-hgis.pdb" "start.bat" "start.ps1" | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy runtime failed $LASTEXITCODE" }
}

Copy-Item $srcExe (Join-Path $out "andong-viewer.exe") -Force
$pdb = Join-Path $root "build\Release\andong-viewer.pdb"
if (Test-Path $pdb) { Copy-Item $pdb $out -Force }

$data = Join-Path $out "data\andong"
New-Item -ItemType Directory -Force -Path $data | Out-Null
Copy-Item (Join-Path $root "data\andong\andong_city.geojson") $data -Force
$mark = Join-Path $root "data\andong\andong_mark.png"
if (Test-Path $mark) { Copy-Item $mark $data -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $out "data\theme") | Out-Null
Copy-Item (Join-Path $root "data\theme\andong.qss") (Join-Path $out "data\theme\andong.qss") -Force

$shp = Join-Path $data "shp"
New-Item -ItemType Directory -Force -Path $shp | Out-Null
$srcShp = Join-Path $root ([string]([char]0xC548) + [string]([char]0xB3D9) + [string]([char]0xC2DC))
if (Test-Path -LiteralPath $srcShp) {
  robocopy $srcShp $shp /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy shp failed $LASTEXITCODE" }
}

$emdDst = Join-Path $data "emd"
New-Item -ItemType Directory -Force -Path $emdDst | Out-Null
$desk = [Environment]::GetFolderPath("Desktop")
$andong = ([string]([char]0xC548) + [string]([char]0xB3D9) + [string]([char]0xC2DC))
$gyeongbuk = ([string]([char]0xACBD) + [string]([char]0xBD81))
$emdSrc = Join-Path $desk ($andong + "\LSMD_ADM_SECT_UMD_" + $gyeongbuk)
if (Test-Path -LiteralPath $emdSrc) {
  robocopy $emdSrc $emdDst /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy emd failed $LASTEXITCODE" }
}

$cadDst = Join-Path $data "cad"
New-Item -ItemType Directory -Force -Path $cadDst | Out-Null
$cadSrc = Join-Path $desk ($andong + "\LSMD_CONT_LDREG_" + $gyeongbuk + "_" + $andong)
if (Test-Path -LiteralPath $cadSrc) {
  robocopy $cadSrc $cadDst /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy cad failed $LASTEXITCODE" }
}

$sat = Join-Path $root "data\andong\satellite.mbtiles"
if (Test-Path $sat) { Copy-Item $sat $data -Force }
$cad = Join-Path $root "data\andong\cadastral.mbtiles"
if (Test-Path $cad) { Copy-Item $cad $data -Force }
$jibun = Join-Path $root "data\andong\jibun.mbtiles"
if (Test-Path $jibun) { Copy-Item $jibun $data -Force }

$batLines = @(
  '@echo off',
  'setlocal',
  'cd /d "%~dp0"',
  'start "" "%~dp0andong-viewer.exe"'
)
$batLines | Set-Content -Path (Join-Path $out "start-andong.bat") -Encoding ASCII

Write-Host "andong portable -> $out"
$exeOut = Get-Item (Join-Path $out "andong-viewer.exe")
Write-Host ("exe bytes=" + $exeOut.Length)
exit 0
