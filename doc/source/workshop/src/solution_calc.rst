:orphan:

.. _solution_calc:

Exercise solution for ``gdal raster calc``
==========================================

1.

::

    $ gdal raster calc --input s2_TER_10m.xml --output ndvi.tif --calc "(X[4] - X[1]) / (X[4] + X[1])"


2.

::

    $ gdal raster select s2_TER_10m.xml red.gdalg.json --band red
    $ gdal raster select s2_TER_10m.xml NIR.gdalg.json --band NIR
    $ gdal raster calc --input red=red.gdalg.json --input NIR=NIR.gdalg.json --output ndvi.gdalg.json  --calc "(NIR - red) / (NIR + red)"

3.

::

    $ gdal pipeline calc --input s2_TER_10m.xml --calc "(X[4] - X[1]) / (X[4] + X[1])" ! write ndvi_generic.gdalg.json

    $ gdal pipeline ndvi_generic.gdalg.json  --calc.input=s2_TES_10m.xml --write.output=NDVI_TES.tif --format COG
