@echo off

set GDAL_DIR=c:\gdal114

if exist %GDAL_DIR%\SETUP_GDAL.BAT goto DIROK

echo -----------------------------------------------------------------------
echo It appears that the GDAL_DIR environment variable is not set properly
echo in SETUP_GDAL.BAT.  Please edit GDAL_SETUP.BAT, and modify the GDAL_DIR
echo variable to contain the directory containing the SETUP_GDAL.BAT ... the
echo base directory of the unzipped GDAL tree.
echo -----------------------------------------------------------------------

goto Done

:DIROK

set PATH=%GDAL_DIR%\bin;%PATH%
set INCLUDE=%GDAL_DIR%\include;%INCLUDE%
set LIB=%GDAL_DIR%\include;%LIB%
set PYTHONPATH=%GDAL_DIR%\pymod;%PYTHONPATH%
set GEOTIFF_CSV=%GDAL_DIR%\data

:Done