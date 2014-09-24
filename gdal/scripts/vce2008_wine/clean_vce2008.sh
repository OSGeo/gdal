#!/bin/sh

if ! test -d $HOME/gdal_vce2008; then
    echo "$HOME/gdal_vce2008 does not exist. Run ./prepare-gdal-vce2008.sh first"
    exit 1
fi

cd $HOME/gdal_vce2008/gdal

wine cmd /c clean_vce2008.bat
