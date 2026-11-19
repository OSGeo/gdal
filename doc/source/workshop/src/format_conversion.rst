.. _format_conversion:

================================================================================
Format conversion
================================================================================

Raster
++++++

Let's try converting a Sentinel 2 dataset to Cloud-optimized GeoTIFF
(:ref:`COG <raster.cog>`) using :ref:`gdal raster convert <gdal_raster_convert>`

::

    $ gdal raster convert S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml TDR.tif --format COG

::

    ERROR 6: COG driver does not support 0-band source raster


OK, we need to select one of the subdatasets:

::

    $ gdal raster convert SENTINEL2_L2A:S2B_MSIL2A_20260423T094029_N0512_R036_T34TDR_20260423T115714.SAFE/MTD_MSIL2A.xml:10m:EPSG_32634 TDR.tif --format COG

Wait ~ 1 minute.

Let's check:

::

    $ gdal info TDR.tif

::

    [ ... snip ... ]
    Image Structure Metadata:
      LAYOUT=COG
      COMPRESSION=LZW
      INTERLEAVE=PIXEL
    [ ... snip ... ]
    Band 1 Block=512x512 Type=UInt16, ColorInterp=Red
      Description = B4, central wavelength 665 nm
      Overviews: 5490x5490, 2745x2745, 1373x1373
    [ ... snip ... ]


Exercise
++++++++

1. Play with the different compression methods and observe effects on image size.

2. Try creating a 8-bit JPEG-compressed tiled GeoTIFF file with the red, green and blue bands.

   .. collapse:: (hint)

       .. hint::

           Use :ref:`gdal raster scale <gdal_raster_scale>`

3. Improve the visual result by reducing the effect of extreme pixel values (outliers) using statistical clamping.

   .. collapse:: (hint)

       .. hint::

           Use :ref:`gdal raster select <gdal_raster_select>`.

           You can improve the visual result by clamping the input range to
           2 standard deviations of the mean.

==> :ref:`solution_format_conversion_raster`.


Vector
++++++

Convert the :file:`timisoara.osm.pbf` to GeoPackage, with the ``other_tags`` field as JSON
using :ref:`gdal vector convert <gdal_vector_convert>`.

::

    $ gdal vector convert timisoara.osm.pbf timisoara.gpkg --open-option TAGS_FORMAT=JSON


Let's check:

::

    $ gdal info timisoara.gpkg

::

    INFO: Open of `timisoara.gpkg'
      using driver `GPKG' successful.

    Layer name: points
    Geometry: Point
    Feature Count: 20415
    Extent: (20.167133, 45.346126) - (22.818334, 46.318563)
    Layer Coordinate Reference System:
      - name: WGS 84
      - ID: EPSG:4326
      - type: Geographic 2D
      - area of use: World, west -180.00, south -90.00, east 180.00, north 90.00
    Data axis to CRS axis mapping: 2,1
    FID Column = fid
    Geometry Column = geom
    osm_id: String (0.0)
    name: String (0.0)
    barrier: String (0.0)
    highway: String (0.0)
    ref: String (0.0)
    address: String (0.0)
    is_in: String (0.0)
    place: String (0.0)
    man_made: String (0.0)
    other_tags: String(JSON) (0.0)
    
    Layer name: lines
    [ ... snip ... ]
    
    Layer name: multilinestrings
    [ ... snip ... ]
    
    Layer name: multipolygons
    [ ... snip ... ]
    
    Layer name: other_relations
    [ ... snip ... ]

Exercise
++++++++

1. Convert :file:`timisoara.osm.pbf` to GeoJSON

   .. collapse:: (hint)

       .. hint::

           Use the ``--input-layer`` argument to generate one output file per input layer.

2. If you open the above :file:`timisoara.gpkg` in QGIS, you can see that the
   ``other_relations`` layer cannot be displayed, because it contains geometry collection
   features that QGIS does not support.

   Try "exploding" those collections in single primitives (points, lines, polygons).

   .. collapse:: (hint)

       .. hint::

            Use :ref:`gdal vector explode-collections <gdal_vector_explode_collections>`.

3. Create a layer for each geometry type in the ``other_collections`` layer.

   .. collapse:: (hint)

       .. hint::

           Use :ref:`gdal vector filter <gdal_vector_filter>`
           and Spatialite `ST_GeometryType <https://www.gaia-gis.it/gaia-sins/spatialite-sql-5.1.0.html#p4>`__ function


==> :ref:`solution_format_conversion_vector`.
