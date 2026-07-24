:orphan:

.. _solution_clip:

Exercise solution for clipping
==============================

::

    gdal vector sql '{"type":"Point","coordinates":[21.2322,45.7558]}' \
        clipping_circle.gpkg --sql "select st_buffer(st_transform(geometry,32634), 1000) from OGRGeoJSON" \
        --dialect sqlite --overwrite

     gdal raster clip \
         SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
         S2_TER_circle.tif \
         --like clipping_circle.gpkg
