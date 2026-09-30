@echo off
REM This is a very primitive script which must be called from a build
REM directory and by default sets the environment for a Release build

set Configuration=%~1
if "%Configuration%"=="" set Configuration=Release

set PATH=%CD%\swig\python\bin;%CD%\apps\%Configuration%;%CD%\%Configuration%;%PATH%
set GDAL_DATA=%CD%\data
set PYTHONPATH=%CD%\swig\python
set GDAL_DRIVER_PATH=%CD%\gdalplugins\%Configuration%