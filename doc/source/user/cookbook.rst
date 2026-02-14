.. _cookbook:

================================================================================
GDAL Command-line Cookbook
================================================================================


.. contents::
    :depth: 3

Introduction
------------

Welcome to the GDAL Users Cookbook. This guide provides practical examples for working with raster and vector data using the GDAL command-line tools.
It demonstrates common tasks such as raster analysis, vector geometry operations, and combining raster and vector workflows.
Examples includes code snippets in both Bash and PowerShell, or links to relevant examples elsewhere in the GDAL documentation.

General
-------

Get the GDAL version
++++++++++++++++++++

.. tabs::

   .. code-tab:: bash

        ogr2ogr --version
        gdalinfo --version

   .. code-tab:: bash gdal CLI

        gdal --version

Raster
------

Resize a Raster
+++++++++++++++

See :ref:`gdal_raster_resize_example_resize`.

Vector
------

Get the geometry field name for a dataset
+++++++++++++++++++++++++++++++++++++++++

After running the commands below look for ``Geometry Column = `` in the output.
For example, if you see ``Geometry Column = geom``, then the geometry field name is ``geom``.

.. tabs::

   .. code-tab:: bash

        # list layers
        ogrinfo -so edges.gpkg
        # list details for a specific layer
        ogrinfo -so edges.gpkg Edges

   .. code-tab:: bash gdal CLI

        gdal vector info edges.gpkg

Buffer geometries
+++++++++++++++++

You can use the :ref:`gdal_vector_buffer` command; see :ref:`gdal_vector_buffer_examples` for usage examples.
Alternatively, the SQLite dialect can be used, as shown below.

Buffer geometries using an attribute
++++++++++++++++++++++++++++++++++++

This example uses a ``lines.gpkg`` dataset containing a single layer named ``lines``,
with a geometry field named ``geom`` and an integer attribute named ``width``. The value
of this attribute is used as the buffer distance for each feature.

.. note::

    When creating derived geometries using SQL, avoid using ``SELECT *``.
    Including the original geometry field will result in multiple geometry
    columns in the output. Instead, explicitly list the required attributes
    and return a single geometry column.

.. tabs::

   .. code-tab:: bash

        ogr2ogr buffered-lines.gpkg lines.gpkg \
            -dialect SQLITE \
            -sql "SELECT fid, ST_Buffer(geom, width) AS geom FROM lines" \
            -overwrite \
            -nlt POLYGON -nln BufferedLines

   .. code-tab:: bash gdal CLI

        gdal vector pipeline \
            ! read lines.gpkg \
            ! sql "SELECT fid, ST_Buffer(geom, width) AS geom FROM lines" \
            ! set-geom-type --geometry-type Polygon \
            ! write buffered-lines.gpkg --output-layer=BufferedLines --overwrite --overwrite-layer

Combined Raster and Vector Workflows
------------------------------------

Burn a vector dataset into a raster
+++++++++++++++++++++++++++++++++++

See :ref:`gdal_vector_rasterize_example_burn`.

Extract pixel values from a raster and apply them to a point dataset
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

Example dataset: ``points.gpkg`` contains a single layer called ``points`` with a geometry column named ``geometry``.

.. tabs::

   .. code-tab:: bash

    export OGR_SQLITE_ALLOW_EXTERNAL_ACCESS="YES"
    gdal vector sql points.gpkg \
        points-dem.gpkg \
        --sql "SELECT *, gdal_get_pixel_value('./dem.tif', 1, 'georef', ST_X(geometry), ST_Y(geometry)) as pixel FROM points" \
        --dialect SQLITE \
        --overwrite

   .. code-tab:: ps1

    $env:OGR_SQLITE_ALLOW_EXTERNAL_ACCESS="YES"
    gdal vector sql points.gpkg `
        points-dem.gpkg `
        --sql "SELECT *, gdal_get_pixel_value('./dem.tif', 1, 'georef', ST_X(geometry), ST_Y(geometry)) as pixel FROM points" `
        --dialect SQLITE `
        --overwrite
