.. _gdal_vector_reproject:

================================================================================
``gdal vector reproject``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Reproject a vector dataset.

.. Index:: gdal vector reproject

Synopsis
--------

.. program-output:: gdal vector reproject --help-doc

Description
-----------

:program:`gdal vector reproject` can be used to reproject a vector dataset.
The program can reproject to any supported projection.

This command can also be used as a step of :ref:`gdal_vector_pipeline`.

.. GDALG output (on-the-fly / streamed dataset)
.. --------------------------------------------

.. include:: gdal_cli_include/gdalg_vector_compatible.rst


Program-Specific Options
------------------------

.. option:: -d, --dst-crs <SRC-CRS>

    Set destination spatial reference.

    .. include:: gdal_options/srs_def_gdal_raster_reproject.rst

.. option:: -s, --src-crs <SRC-CRS>

    Set source spatial reference. If not specified the SRS found in the input
    dataset will be used.

    .. include:: gdal_options/srs_def_gdal_raster_reproject.rst

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/active_layer.rst

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/input_layer.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst

    .. include:: gdal_options/quiet.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Reproject a GeoPackage file to CRS EPSG:32632 ("WGS 84 / UTM zone 32N")

   .. code-block:: bash

        $ gdal vector reproject --dst-crs=EPSG:32632 in.gpkg out.gpkg --overwrite

.. example::
   :id: gdal-vector-reproject-crs
   :title: Reproject data using various CRS formats

   The following examples demonstrate different ways to specify a coordinate reference system.
   Each command reprojects the data to the Web Mercator (EPSG:3857) projection and produces identical output.

   .. code-block:: bash

        # OGC CRS URI
        $ gdal vector reproject \
            --dst-crs="http://www.opengis.net/def/crs/EPSG/0/3857" \
            natural_earth_vector.gpkg --layer=ne_10m_populated_places \
            places.json --overwrite

        # OGC CRS URN
        $ gdal vector reproject \
            --dst-crs="urn:ogc:def:crs:EPSG::3857" \
            natural_earth_vector.gpkg --layer=ne_10m_populated_places \
            places.json --overwrite

        # PROJ string (legacy format)
        $ PROJ4="+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +nadgrids=@null +wktext +no_defs +type=crs"
        $ gdal vector reproject \
            --dst-crs="$PROJ4" \
            natural_earth_vector.gpkg --layer=ne_10m_populated_places \
            places.json --overwrite

.. example::
   :title: Reproject a layer in a GeoPackage to Web Mercator GeoJSON

   The timezone layer cannot be fully reprojected to Web Mercator as the coordinates at the poles fall outside the extent of the Web Mercator bounding-box causing
   some features to be missing in the output. The following errors are returned:

   .. code-block:: bash

        ERROR 1: Full reprojection failed, but partial is possible if you define OGR_ENABLE_PARTIAL_REPROJECTION configuration option to TRUE
        ERROR 1: PROJ: webmerc: Invalid latitude
        ERROR 1: Reprojection failed, err = 2049, further errors will be suppressed on the transform object.

   You can follow the suggestion above and enable ``OGR_ENABLE_PARTIAL_REPROJECTION``.
   Errors from PROJ relating to invalid coordinates (``ERROR 1: PROJ: webmerc: Invalid latitude``) will still be reported, but all features will be written to the output.

   .. code-block:: bash

        $ gdal vector reproject \
            --dst-crs=EPSG:3857 \
            --config OGR_ENABLE_PARTIAL_REPROJECTION=TRUE \
            natural_earth_vector.gpkg --layer=ne_10m_time_zones \
            ne_10m_time_zones.json \
            --overwrite

