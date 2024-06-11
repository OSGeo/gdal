@echo off

REM This is a very primitive script which must be called from a build
REM directory and set the environment for a Release build

set PATH=%CD%\swig\python\bin;%CD%\apps\Release;%CD%\Release;%PATH%
set GDAL_DATA=%CD%\data
set PYTHONPATH=%CD%\swig\python
set GDAL_DRIVER_PATH=%CD%\gdalplugins\Release
set USE_PATH_FOR_GDAL_PYTHON=yes
