#!/bin/bash
# Load test data in PostGIS
# Assume RASTER_OVERVIEWS table created. If not, add -V option to one of the
# instructions, apart from the one with level 1 (-l 1)

# CHANGE THIS TO MATCH YOUR ENVIROMENT
GDAL2WKTRASTER_PATH=/home/jorge/src/wktraster/scripts
IMG_TEST_FILES_PATH=/home/jorge/test_data/tiff_files
SQL_OUTPUT_FILES_PATH=/home/jorge/test_data/sql

# We add -V option to create the RASTER_OVERVIEWS table. Only one time.

$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t utm -l 1 -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level1.sql -s 26711 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t utm -l 2 -V -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level2.sql -s 26711 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t utm -l 4 -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level4.sql -s 26711 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t utm -l 8 -k 100x100 -o $SQL_OUTPUT_FILES_PATH/utm_level8.sql -s 26711 -I -M
#$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/utm.tif -t utm -l 1 -k 100x100 -R -o $SQL_OUTPUT_FILES_PATH/utm_outdb_level1.sql -s 26711 -I -M

psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/utm_level1.sql
psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/utm_level2.sql
psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/utm_level4.sql
psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/utm_level8.sql

# Out-db support is not working in WKT Raster right now (August 17th 2009). To allow out-db rasters in AddRasterColumns function, you must change
# the $WKTRASTER_SRC/rt_pg/rtpostgis.sql code, comment lines 532 - 535 (verify out_db), and execute rtpostgis.sql again in the same database
#psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/utm_outdb_level1.sql


$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t small_world -l 1 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level1.sql -s 4326 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t small_world -l 2 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level2.sql -s 4326 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t small_world -l 4 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level4.sql -s 4326 -I -M
$GDAL2WKTRASTER_PATH/gdal2wktraster.py -r $IMG_TEST_FILES_PATH/small_world.tif -t small_world -l 8 -k 40x20 -o $SQL_OUTPUT_FILES_PATH/small_world_level8.sql -s 4326 -I -M


psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/small_world_level1.sql
psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/small_world_level2.sql
psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/small_world_level4.sql
psql -h localhost -U postgres -d gdal_wktraster_test -f $SQL_OUTPUT_FILES_PATH/small_world_level8.sql
