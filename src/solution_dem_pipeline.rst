:orphan:

.. _solution_dem_pipeline:

Exercise solution for DEM pipeline
==================================

1. Modified pipeline to do hillshaded before hypsometric rendering

::

    $ gdal raster pipeline \
        read dem.tif ! \
        hillshade ! \
        blend --input [ read dem.tif color-map --color-map test.cpt ] --overlay _PIPE_ --operator hsv-value ! \
        write out.tif --overwrite



2. Generate colorized and hillshaded maps as intermediate results

::

    $ gdal raster pipeline \
        read dem.tif ! \
        color-map --color-map test.cpt ! \
        tee [ write dem_colorized_tee.tif ] ! \
        blend [ read dem.tif ! hillshade ! tee [ write dem_hillshade.tif ] ] --operator hsv-value ! \
        write out.tif --overwrite

