#!/bin/sh
gdal_translate ../gcore/data/byte.tif byte_gmljp2_v2.jp2 -of JP2OPENJPEG -co GMLJP2V2_DEF=YES

