:orphan:

.. _solution_materialize:

Exercise solution for materializing pipeline intermediate result
================================================================

::

    $ gdal raster pipeline \
            mosaic SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
            SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
            SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TES_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
            ! \
            select --band 1,2,3 \
            ! \
            scale --input-min 400 \
                  --input-max 2400 \
                  --output-data-type uint8 \
            ! \
            materialize --output=mosaic.tif \
            ! \
            tile --min-zoom 10 s2_tiled_min_zoom10 --format WEBP


.. note::

      Original idea was to use COG output instead of regular GeoTIFF, but fails
      currently because of https://github.com/OSGeo/gdal/issues/14730
