$ErrorActionPreference = "Stop"
# Existence-based OSGeo root: env → C:\OSGeo4W → D:\OSGeo4W → A:\OSGeo4W
$OSGEO = $null
if ($env:OSGEO4W_ROOT -and (Test-Path -LiteralPath $env:OSGEO4W_ROOT)) {
  $OSGEO = $env:OSGEO4W_ROOT
} elseif (Test-Path -LiteralPath "C:\OSGeo4W") {
  $OSGEO = "C:\OSGeo4W"
} elseif (Test-Path -LiteralPath "D:\OSGeo4W") {
  $OSGEO = "D:\OSGeo4W"
} elseif (Test-Path -LiteralPath "A:\OSGeo4W") {
  $OSGEO = "A:\OSGeo4W"
} else {
  throw "OSGEO4W_ROOT not found. Set OSGEO4W_ROOT or install OSGeo4W at C:\OSGeo4W (preferred), D:\OSGeo4W, or A:\OSGeo4W."
}
$env:OSGEO4W_ROOT = $OSGEO
$qgis = Join-Path $OSGEO "apps\qgis-dev"
if (-not (Test-Path $qgis)) { throw "qgis-dev missing under $OSGEO" }
$env:QGIS_PREFIX_PATH = $qgis
$bins = @(
  (Join-Path $qgis "bin"),
  (Join-Path $OSGEO "apps\Qt6\bin"),
  (Join-Path $OSGEO "apps\gdal-dev\bin"),
  (Join-Path $OSGEO "apps\pdal-dev\bin"),
  (Join-Path $OSGEO "bin")
)
$env:PATH = ($bins + $env:PATH) -join ";"
$env:QT_PLUGIN_PATH = (Join-Path $OSGEO "apps\Qt6\plugins") + ";" + (Join-Path $qgis "qtplugins")
$env:QGIS_PLUGIN_PATH = Join-Path $qgis "plugins"
$gdalData = Join-Path $OSGEO "apps\gdal-dev\share\gdal"
if (Test-Path $gdalData) { $env:GDAL_DATA = $gdalData }
$proj = Join-Path $OSGEO "share\proj"
if (Test-Path $proj) { $env:PROJ_DATA = $proj; $env:PROJ_LIB = $proj }
$env:CMAKE_BUILD_PARALLEL_LEVEL = "$([Environment]::ProcessorCount)"
$env:CTEST_PARALLEL_LEVEL = "$([Environment]::ProcessorCount)"
Write-Host "OSGEO4W_ROOT=$env:OSGEO4W_ROOT"
Write-Host "QGIS_PREFIX_PATH=$env:QGIS_PREFIX_PATH"
Write-Host "PARALLEL_JOBS=$env:CMAKE_BUILD_PARALLEL_LEVEL (CPU cores)"
