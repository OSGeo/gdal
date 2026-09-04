# This is a very simple script which must be called from a build
# directory and by default sets the environment for a Release build.

param(
    [string]$Configuration = "Release"
)

$env:PATH = "$PWD\swig\python\bin;$PWD\apps\$Configuration;$PWD\$Configuration;$env:PATH"
$env:GDAL_DATA = "$PWD\data"
$env:PYTHONPATH = "$PWD\swig\python"
$env:GDAL_DRIVER_PATH = "$PWD\gdalplugins\$Configuration"