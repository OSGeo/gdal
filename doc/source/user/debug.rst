=================================
GDAL CLI Debug and Logging Output
=================================

.. contents::
    :depth: 3

Debug Output
------------

To display GDAL debug output for a command, use the ``--debug`` option.
See :ref:`raster_common_options` and :ref:`vector_common_options` for details on ``--debug``.
The following example enables debug output for :ref:`gdal_vector_info`.

.. code-block:: console

    $ gdal vector info in.gpkg --debug ON

When ``debug`` is enabled, GDAL writes additional messages to the command error stream.
These messages can help identify issues related to dataset access, driver loading,
configuration options, and processing operations.

Each debug message includes a prefix indicating the component that produced it.
The prefixes typically indicate drivers (e.g. ``Shape``, ``GeoJSON``), tools (e.g. ``GDALVectorTranslate``),
or the core GDAL runtime (``GDAL``).

.. example::
   :title: Convert vector datasets with debug output
   :id: debug-output

    The following example shows debug output with different prefix types.

    .. code-block:: console

       $ gdal vector convert in.shp out.geojson --overwrite --debug ON
       GDAL: GDALOpen(in.shp, this=000001719D0A3D80) succeeds as ESRI Shapefile.
       GeoJSON: First pass: 100.00 %
       GDAL: GDALOpen(out.geojson, this=000001719D0AC240) succeeds as GeoJSON.
       GDAL: GDALClose(out.geojson, this=000001719D0AC240)
       GDAL: Using GeoJSON driver
       GDAL: GDALDriver::Create(GeoJSON,out.geojson,0,0,0,Unknown,0000000000000000)
       0...10...20...30...40...50...60...70...80...90...100 - done.
       GDALVectorTranslate: 19 features written in layer 'in'
       Shape: 19 features read on layer 'in'.

Debug output can be filtered by passing a component prefix to the ``--debug`` option.
For example, to display only messages beginning with ``Shape:`, use ``--debug Shape``.
Prefix matching is case-insensitive, but partial matches and wildcards are not supported.

.. example::
   :title: Filter debug output by driver

    .. code-block:: console

       $ gdal vector convert in.shp out.geojson --overwrite --debug Shape
       0...10...20...30...40...50...60...70...80...90...100 - done.
       Shape: 19 features read on layer 'in'.

.. note::

   The ``--debug`` option accepts ``ON`` to enable debug output, ``OFF`` to disable it, or a prefix string
   to filter debug messages. If none of these values are provided, GDAL returns the following error:

   .. code-block:: console

      ERROR 1: --debug option given without debug level.

Logging and Diagnostic Options
------------------------------

There are several configuration options that control GDAL logging output.
These are documented in the :ref:`configoptions_logging` section of the config options page.

The options are prefixed with :term:`CPL` ("Common Portability Library"), the internal GDAL utility library used by GDAL core and drivers.

Options can be set per command using the ``--config`` option, or globally using environment variables.
Note that ``--debug ON`` is equivalent to ``--config CPL_DEBUG=ON``.

The following are all broadly equivalent, although environment variables affect all subsequent GDAL commands in the same session, whereas ``--config`` only applies to the current command.

.. tabs::

    .. code-tab:: bash

        $ gdal vector info in.gpkg --debug ON
        $ gdal vector info in.gpkg --config CPL_DEBUG=ON

        # set CPL_DEBUG for the scope of the single command
        $ CPL_DEBUG=ON gdal vector info in.gpkg

        # set CPL_DEBUG for the whole shell session
        $ export CPL_DEBUG=ON
        $ gdal vector info in.gpkg

    .. code-tab:: ps1

        gdal vector info in.gpkg --debug ON
        gdal vector info in.gpkg --config CPL_DEBUG=ON

        # set CPL_DEBUG for the whole shell session
        $env:CPL_DEBUG="ON"
        gdal vector info in.gpkg

When working with remote datasets, it is often useful to log :term:`curl` output for HTTP requests. This can be done by setting the :config:`CPL_CURL_VERBOSE` configuration option to ``ON``.

.. example::
   :title: Get raster information with HTTP request logging enabled
   :id: debug-log-curl

   .. code-block:: console

      $ gdal raster info /vsicurl/https://download.osgeo.org/geotiff/samples/spot/chicago/SP27GTIF.TIF --config CPL_CURL_VERBOSE=ON

:config:`CPL_TIMESTAMP` adds timestamps to each log and debug message emitted by GDAL, which can be useful for troubleshooting performance bottlenecks and benchmarking.

.. example::
   :title: Get raster info with curl logging and timestamps

   .. tabs::

      .. code-tab:: bash

        $ export CPL_TIMESTAMP=ON
        $ gdal raster info /vsicurl/https://download.osgeo.org/geotiff/samples/spot/chicago/SP27GTIF.TIF --debug "CURL_INFO_TEXT" --config CPL_CURL_VERBOSE=ON
        [Fri May 15 11:57:24 2026].6370, 0.0000: CURL_INFO_TEXT: Could not find host download.osgeo.org in the .netrc file; using defaults
        [Fri May 15 11:57:24 2026].6640, 0.0270: CURL_INFO_TEXT: Host download.osgeo.org:443 was resolved.

      .. code-tab:: ps1

        $env:CPL_TIMESTAMP="ON"
        gdal raster info /vsicurl/https://download.osgeo.org/geotiff/samples/spot/chicago/SP27GTIF.TIF --debug "CURL_INFO_TEXT" --config CPL_CURL_VERBOSE=ON

:config:`CPL_LOG_ERRORS` enables logging of error messages through GDAL’s CPL logging system, rather than only reporting them to the terminal.
When combined with :config:`CPL_LOG`, log output (including errors) can be written to a file.

.. example::
   :title: Log debug output and errors to a file
   :id: debug-log-file

   .. tabs::

      .. code-tab:: bash

        export CPL_DEBUG=ON
        export CPL_LOG_ERRORS=ON
        export CPL_TIMESTAMP=ON
        gdal raster info /vsicurl/https://download.osgeo.org/geotiff/samples/spot/chicago/SP27GTIF.TIF --config CPL_LOG=/tmp/gdal.log

      .. code-tab:: ps1

        $env:CPL_DEBUG="ON"
        $env:CPL_LOG_ERRORS="ON"
        $env:CPL_TIMESTAMP="ON"
        gdal raster info /vsicurl/https://download.osgeo.org/geotiff/samples/spot/chicago/SP27GTIF.TIF --config CPL_LOG=C:\Temp\gdal.log

