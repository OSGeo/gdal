:orphan:

.. _solution_raster_info:

Exercise solution for raster info
=================================

::

    $ gdal raster info SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 --format=json  | jq ".stac[\"proj:epsg\"]"

Output:

::

    32634
