.. _migration_guide_to_gdal_cli:

================================================================================
Migration guide to "gdal" command line interface
================================================================================

This page documents through examples how to migrate from the traditional GDAL
command line utilities to the unified "gdal" command line interface added in
GDAL 3.11.

Raster commands
---------------

* Getting information on a raster dataset in human-readable format

.. code-block::

    gdalinfo my.tif

    ==>

    gdal raster info my.tif


* Converting a georeferenced netCDF file to cloud-optimized GeoTIFF

.. code-block::

    gdal_translate -of COG in.nc out.tif

    ==>

    gdal raster convert --of=COG in.nc out.tif


* Reprojecting a GeoTIFF file to a Deflate compressed tiled GeoTIFF file

.. code-block::

    gdalwarp -t_srs EPSG:4326 -co TILED=YES -co COMPRESS=DEFLATE -overwrite in.tif out.tif

    ==>

    gdal raster reproject --dst-crs=EPSG:4326 --co=TILED=YES,COMPRESS=DEFLATE --overwrite in.tif out.tif


* Update existing out.tif with content of in.tif using cubic interpolation

.. code-block::

    gdalwarp -r cubic in.tif out.tif

    ==>

    gdal raster update -r cubic in.tif out.tif


* Converting a PNG file to a tiled GeoTIFF file, adding georeferencing for world coverage in WGS 84 and metadata

.. code-block::

    gdal_translate -a_ullr -180 90 180 -90 -a_srs EPSG:4326 -co TILED=YES -mo DESCRIPTION=Mean_day_temperature in.png out.tif

    ==>

    gdal raster pipeline read in.png ! edit --crs=EPSG:4326 --bbox=-180,-90,180,90 --metadata=DESCRIPTION=Mean_day_temperature ! write --co=TILED=YES out.tif

Note that the order of elements differ: "upper-left-x upper-left-y lower-right-x lower-right-y" for gdal_translate,
compared to "minimum-x,minimum-y,maximum-x,maximum-y" for the ``--bbox`` option of "gdal raster pipeline ... edit".


* Clipping a raster with a bounding box

.. code-block::

    gdal_translate -projwin 2 50 3 49 in.tif out.tif

    ==>

    gdal raster clip --bbox=2,49,3,50 in.tif out.tif


* Creating a virtual mosaic (.vrt) from all GeoTIFF files in a directory

.. code-block::

    gdalbuildvrt out.vrt src/*.tif

    ==>

    gdal raster mosaic src/*.tif out.vrt


* Creating a mosaic in COG format from all GeoTIFF files in a directory

.. code-block::

    gdalbuildvrt tmp.vrt src/*.tif
    gdal_translate -of COG tmp.vrt out.tif

    ==>

    gdal raster mosaic --of=COG src/*.tif out.tif


* Adding internal overviews for reduction factors 2, 4, 8 and 16 to a GeoTIFF file

.. code-block::

    gdaladdo -r average my.tif 2 4 8 16

    ==>

    gdal raster overview add -r average --levels=2,4,8,16 my.tif


* Combining single-band rasters into a multi-band raster

.. code-block::

    gdalbuildvrt tmp.vrt red.tif green.tif blue.tif
    gdal_translate tmp.vrt out.tif

    ==>

    gdal raster stack red.tif green.tif blue.tif out.tif


* Reorder a 3-band dataset with bands ordered Blue, Green, Red to Red, Green, Blue

.. code-block::

    gdal_translate -b 3 -b 2 -b 1 bgr.tif rgb.tif

    ==>

    gdal raster select --band 3,2,1 bgr.tif rgb.tif --overwrite


* Expand a dataset with a color table to RGB

.. code-block::

    gdal_translate -expand rgb color_table.tif rgb.tif

    ==>

    gdal raster color-map color_table.tif rgb.tif --overwrite


* Apply an external color-map to a dataset

.. code-block::

    gdaldem color-map color_table.tif color_map.txt rgb.tif

    ==>

    gdal raster color-map --color-map=color_map.txt color_table.tif rgb.tif --overwrite


* Convert nearly black values of the collar to black

.. code-block::

    nearblack -nb 1 -near 10 my.tif

    ==>

    gdal raster clean-collar --update --color-threshold=1 --pixel-distance=10 my.tif


* Generating tiles between zoom level 2 and 5 of WebMercator from an input GeoTIFF

.. code-block::

     gdal2tiles --zoom=2-5 input.tif output_folder

     ==>

     gdal raster tile --min-zoom=2 --max-zoom=5 input.tif output_folder


Vector commands
---------------

* Getting information on a vector dataset in human-readable format

.. code-block::

    ogrinfo -al -so my.gpkg

    ==>

    gdal vector info my.gpkg


* Converting a shapefile to a GeoPackage

.. code-block::

    ogr2ogr out.gpkg in.shp

    ==>

    gdal vector convert in.shp out.gpkg


* Reprojecting a shapefile to a GeoPackage

.. code-block::

    ogr2ogr -t_srs EPSG:4326 out.gpkg in.shp

    ==>

    gdal vector reproject --dst-crs=EPSG:4326 in.shp out.gpkg


* Clipping a GeoPackage file

.. code-block::

    ogr2ogr -clipsrc 2 49 3 50 out.gpkg in.shp

    ==>

    gdal vector clip --bbox=2,49,3,50 in.gpkg out.gpkg


* Selecting features from a GeoPackage file intersecting a bounding box, but not clipping them to it

.. code-block::

    ogr2ogr -spat 2 49 3 50 out.gpkg in.shp

    ==>

    gdal vector filter --bbox=2,49,3,50 in.gpkg out.gpkg


*  Selecting features from a shapefile intersecting a bounding box, but not clipping them to it and reprojecting

.. code-block::

    ogr2ogr -t_srs EPSG:32631 -spat 2 49 3 50 out.gpkg in.shp

    ==>

    gdal vector pipeline read in.gpkg ! filter --bbox=2,49,3,50 ! reproject --dst-crs=EPSG:32631 ! write out.gpkg


* Selecting features from a shapefile based on an attribute query, and restricting to a few fields

.. code-block::

    ogr2ogr -where "country='Greenland'" -select population,_ogr_geometry_ out.gpkg in.shp

    ==>

    gdal vector pipeline ! read in.shp ! filter --where "country='Greenland'" ! select --fields population,_ogr_geometry_ ! write out.gpkg


* Creating a GeoPackage stacking all input shapefiles in separate layers.

.. code-block::

    ogrmerge -f GPKG -o merged.gpkg *.shp

    ==>

    gdal vector concat --mode=stack *.shp merged.gpkg

* Modify in-place a GeoPackage dataset by running a SQL command.

.. code-block::

    ogrinfo my.gpkg -sql "DELETE FROM countries WHERE pop > 1e6"

    ==>

    gdal vector sql --update my.gpkg --sql "DELETE FROM countries WHERE pop > 1e6"

Python commands
---------------

The following commands were previously provided as individual :ref:`Python scripts <python_samples>`, but are now accessible through the unified "gdal" command-line interface.

* Listing the contents of a remote :ref:`vector.pmtiles` dataset, using :ref:`gdal_vsi_list`.

.. code-block::

    python gdal_ls.py -lr "/vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles"

    ==>

    gdal vsi list -lR "/vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles"

* Listing the contents of a vsis3 dataset.

.. code-block::

    export AWS_NO_SIGN_REQUEST="YES"
    gdal_ls.py "/vsis3/overturemaps-us-west-2/release/2025-10-22.0/theme=buildings/type=building"

    ==>

    export AWS_NO_SIGN_REQUEST="YES"
    gdal vsi list "/vsis3/overturemaps-us-west-2/release/2025-10-22.0/theme=buildings/type=building"

* Extracting all remote content into a local directory, using :ref:`gdal_vsi_copy`.

.. code-block::

    gdal_cp.py "/vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles/metadata.json" /vsistdout/ | jq

    ==>

    gdal vsi copy -r "/vsipmtiles//vsicurl/https://protomaps.github.io/PMTiles/protomaps(vector)ODbL_firenze.pmtiles" out_pmtiles
