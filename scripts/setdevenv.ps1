# This is a very simple script which must be called from a build
# directory and sets the environment for a Release build

$env:PATH = "$PWD\swig\python\bin;$PWD\apps\Release;$PWD\Release;" + $env:PATH
$env:GDAL_DATA = "$PWD\data"
$env:PYTHONPATH = "$PWD\swig\python"
$env:GDAL_DRIVER_PATH = "$PWD\gdalplugins\Release"
$env:USE_PATH_FOR_GDAL_PYTHON = "yes"
