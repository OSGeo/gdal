dmake -f Makefile_Geo__GDAL %1

if "%1" == "test" goto clean
if "%1" == "install" goto end

dmake -f Makefile_Geo__GDAL__Const %1
dmake -f Makefile_Geo__OGR %1
dmake -f Makefile_Geo__OSR %1

if "%1" == "clean" goto clean

goto end

:clean
echo off
del tmp_ds_*
del *.gmt
rd tmp_ds_ESRIShapefile /S /Q
rd tmp_ds_MapInfoFile /S /Q
rd tmp_ds_KML /S /Q
rd tmp_ds_BNA /S /Q
rd tmp_ds_GMT /S /Q
rd tmp_ds_GPX /S /Q
rd tmp_ds_GeoJSON /S /Q

if "%1" == "test" goto end

rem del *.c
rem del *.cpp
del *.old
rem rd lib /S /Q

:end
