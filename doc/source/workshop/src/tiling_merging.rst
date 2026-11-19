.. tiling_merging:

================================================================================
Tiling, merging
================================================================================

Raster mosaic
-------------

Let's create a virtual mosaic in :ref:`VRT <raster.vrt>` format using :ref:`gdal raster mosaic <gdal_raster_mosaic>`

::

    # run from the workshop data directory

    $ gdal raster mosaic \
        --input-nodata 0 \
        SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
        SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TER_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
        SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TES_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 \
        s2.vrt

Let's add overviews:

::

    $ gdal raster overview add -r cubic s2.vrt

This generates a :file:`.vrt.ovr` file

Let's open :file:`s2.vrt` in QGIS

Raster tiling
-------------

Using :ref:`gdal raster tile <gdal_raster_tile>`

::

    $ gdal raster tile s2.vrt s2_tiled --format JPEG


::

    ERROR 6: tile: Only up to 4 bands supported for JPEG (with alpha ignored).


Preliminary operations:

::

    $ gdal raster select s2.vrt s2_rgb.vrt --band 1,2,3
    $ gdal raster scale s2_rgb.vrt s2_rgb_clamped.vrt \
            --input-min 400 \
            --input-max 2400 \
            --output-data-type uint8


Now let's generate the individual files:

::

    $ gdal raster tile s2_rgb_clamped.vrt s2_tiled --format JPEG


and check the result (if Firefox is not installed, manually open the file in the browser):

::

    $ firefox s2_tiled/leaflet.html


Doing everything at the same time using a pipeline
--------------------------------------------------

Using :ref:`gdal raster pipeline <gdal_raster_pipeline>`

::

    $ gdal pipeline \
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
            tile --min-zoom 10 s2_tiled_min_zoom10 --format WEBP

.. only:: html

   .. image:: ../images/tile_merging.svg
      :width: 0
      :height: 0

   .. raw:: html

      <object type="image/svg+xml"
              data="../_images/tile_merging.svg">
      </object>

.. only:: not html

   .. image:: ../images/tile_merging.svg

and check the result:

::

    $ firefox s2_tiled_min_zoom10/openlayers.html


Exercise
--------

Modify above pipeline to materialize the scaled dataset as a GeoTIFF before tiling.

.. collapse:: (hint)

   .. hint::

        Use :ref:`gdal raster materialize <gdal_raster_materialize>`


==> :ref:`solution_materialize`.


Vector partitioning
--------------------

Using :ref:`gdal vector partition <gdal_vector_partition>`

First let's create a GeoPackage file from the original OSM data with a slightly
customized import configuration with the OSM tag ``amenity`` being exposed as
a top-level GDAL attribute:

::

    $ gdal vector convert timisoara.osm.pbf --layer points --oo CONFIG_FILE=osm_conf_amenity.ini amenity.gpkg

And let's create a partition around the values of that field:

::

    $ gdal vector partition amenity.gpkg --field amenity amenity_partitioned

    $ gdal vsi ls -lR amenity_partitioned


::

    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bar
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=bar/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=place_of_worship
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=place_of_worship/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=nightclub
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=nightclub/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=loading_dock
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=loading_dock/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=events_venue
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=events_venue/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=shelter
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=shelter/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=payment_terminal
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=payment_terminal/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=fast_food
    -rw-r--r-- 1 unknown unknown       131072 2026-05-21 18:30 points/amenity=fast_food/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=veterinary
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=veterinary/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=childcare
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=childcare/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bus_station
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=bus_station/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=pharmacy
    -rw-r--r-- 1 unknown unknown       139264 2026-05-21 18:30 points/amenity=pharmacy/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=casino
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=casino/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=smoking_area
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=smoking_area/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=recycling
    -rw-r--r-- 1 unknown unknown       270336 2026-05-21 18:30 points/amenity=recycling/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=telephone
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=telephone/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=cinema
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=cinema/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=waste_transfer_station
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=waste_transfer_station/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=waste_basket
    -rw-r--r-- 1 unknown unknown       126976 2026-05-21 18:30 points/amenity=waste_basket/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=ice_cream
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=ice_cream/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=university
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=university/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=conference_centre
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=conference_centre/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=dancing_school
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=dancing_school/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=vehicle_inspection
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=vehicle_inspection/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=parcel_locker
    -rw-r--r-- 1 unknown unknown       135168 2026-05-21 18:30 points/amenity=parcel_locker/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=marketplace
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=marketplace/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=dentist
    -rw-r--r-- 1 unknown unknown       110592 2026-05-21 18:30 points/amenity=dentist/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bank
    -rw-r--r-- 1 unknown unknown       139264 2026-05-21 18:30 points/amenity=bank/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=clock
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=clock/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=gambling
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=gambling/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=studio
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=studio/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=post_box
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=post_box/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=kindergarten
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=kindergarten/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=charging_station
    -rw-r--r-- 1 unknown unknown       110592 2026-05-21 18:30 points/amenity=charging_station/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=school
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=school/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=courthouse
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=courthouse/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=language_school
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=language_school/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=pub
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=pub/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=vending_machine
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=vending_machine/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=arts_centre
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=arts_centre/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=fuel
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=fuel/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=ferry_terminal
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=ferry_terminal/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=public_bookcase
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=public_bookcase/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=clinic
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=clinic/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=community_centre
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=community_centre/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=doctors
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=doctors/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=social_facility
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=social_facility/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=car_wash
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=car_wash/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=parking
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=parking/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=food_court
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=food_court/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bicycle_parking
    -rw-r--r-- 1 unknown unknown       110592 2026-05-21 18:30 points/amenity=bicycle_parking/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=theatre
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=theatre/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=post_office
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=post_office/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=funeral_hall
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=funeral_hall/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=car_rental
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=car_rental/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=toilets
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=toilets/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=waste_disposal
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=waste_disposal/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=payment_centre
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=payment_centre/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bicycle_rental
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=bicycle_rental/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=fountain
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=fountain/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=atm
    -rw-r--r-- 1 unknown unknown       114688 2026-05-21 18:30 points/amenity=atm/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=__HIVE_DEFAULT_PARTITION__
    -rw-r--r-- 1 unknown unknown      3194880 2026-05-21 18:30 points/amenity=__HIVE_DEFAULT_PARTITION__/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=biergarten
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=biergarten/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=taxi
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=taxi/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=disused
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=disused/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bureau_de_change
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=bureau_de_change/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=veterinary_pharmacy
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=veterinary_pharmacy/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=townhall
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=townhall/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=library
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=library/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=drinking_water
    -rw-r--r-- 1 unknown unknown       118784 2026-05-21 18:30 points/amenity=drinking_water/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=police
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=police/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=hospital
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=hospital/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bench
    -rw-r--r-- 1 unknown unknown       180224 2026-05-21 18:30 points/amenity=bench/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=bicycle_repair_station
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=bicycle_repair_station/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=restaurant
    -rw-r--r-- 1 unknown unknown       147456 2026-05-21 18:30 points/amenity=restaurant/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=driving_school
    -rw-r--r-- 1 unknown unknown        98304 2026-05-21 18:30 points/amenity=driving_school/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=parking_entrance
    -rw-r--r-- 1 unknown unknown       106496 2026-05-21 18:30 points/amenity=parking_entrance/part_0000000001.gpkg
    drwxr-xr-x 1 unknown unknown         4096 2026-05-21 18:30 points/amenity=cafe
    -rw-r--r-- 1 unknown unknown       126976 2026-05-21 18:30 points/amenity=cafe/part_0000000001.gpkg

Vector concatenation
--------------------

Using :ref:`gdal vector concat <gdal_vector_concat>`

::

  --mode <MODE>                                              Determine the strategy to create output layers from source layers . MODE=merge-per-layer-name|stack|single (default: merge-per-layer-name)
  --output-layer <OUTPUT-LAYER>                              Name of the output vector layer (single mode), or template to name the output vector layers (stack mode)
  --source-layer-field-name <SOURCE-LAYER-FIELD-NAME>        Name of the new field to add to contain identification of the source layer, with value determined from 'source-layer-field-content'
  --source-layer-field-content <SOURCE-LAYER-FIELD-CONTENT>  A string, possibly using {AUTO_NAME}, {DS_NAME}, {DS_BASENAME}, {DS_INDEX}, {LAYER_NAME}, {LAYER_INDEX}
  --field-strategy <FIELD-STRATEGY>                          How to determine target fields from source fields. FIELD-STRATEGY=union|intersection (default: union)
  -s, --input-crs <INPUT-CRS>                                Input CRS
  -d, --output-crs <OUTPUT-CRS>                              Output CRS


Example:

::

    $ gdal vector concat $(find amenity_partitioned -name "*.gpkg") concatenated.gpkg

