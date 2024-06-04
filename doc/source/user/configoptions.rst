.. _configoptions:

================================================================================
Configuration options
================================================================================

This page discusses runtime configuration options for GDAL. These are distinct from
options to the build-time configure script. Runtime configuration options apply
on all platforms, and are evaluated at runtime. They can be set programmatically,
by commandline switches or in the environment by the user.

Configuration options are normally used to alter the default behavior of GDAL/OGR
drivers and in some cases the GDAL/OGR core. They are essentially global
variables the user can set.

How to set configuration options?
----------------------------------

One example of a configuration option is the :config:`GDAL_CACHEMAX`
option. It controls the size
of the GDAL block cache, in megabytes. It can be set in the environment on Unix
(bash/bourne) shell like this:

::

    export GDAL_CACHEMAX=64


In a DOS/Windows command shell it is done like this:

::

    set GDAL_CACHEMAX=64

It can also be set on the commandline for most GDAL and OGR utilities with the
``--config`` switch, though in a few cases these switches are not evaluated in
time to affect behavior.

::

    gdal_translate --config GDAL_CACHEMAX 64 in.tif out.tif

Since GDAL 3.9, it is also possible to set a config option in a more conventional
way by using a single ``<NAME>``=``<VALUE>`` command line string instead of having ``<NAME>``
and ``<VALUE>`` as two space-separated strings.

::

    gdal_translate --config GDAL_CACHEMAX=64 in.tif out.tif

In C/C++ configuration switches can be set programmatically with
:cpp:func:`CPLSetConfigOption`:

.. code-block:: c

    #include "cpl_conv.h"
    ...
        CPLSetConfigOption( "GDAL_CACHEMAX", "64" );

Normally a configuration option applies to all threads active in a program, but
they can be limited to only the current thread with
:cpp:func:`CPLSetThreadLocalConfigOption`

.. code-block:: c

    CPLSetThreadLocalConfigOption( "GTIFF_DIRECT_IO", "YES" );

For boolean options, the values YES, TRUE or ON can be used to turn the option on;
NO, FALSE or OFF to turn it off.


.. _gdal_configuration_file:

GDAL configuration file
-----------------------

.. versionadded:: 3.3

On driver registration, loading of configuration is attempted from a set of
predefined files.

The following locations are tried by :cpp:func:`CPLLoadConfigOptionsFromPredefinedFiles`:

 - the location pointed by the environment variable (or configuration option)
   :config:`GDAL_CONFIG_FILE` is attempted first. If it is set, the next steps are not
   attempted

 - for Unix builds, the location pointed by ${sysconfdir}/gdal/gdalrc is first
   attempted (where ${sysconfdir} evaluates to ${prefix}/etc, unless the
   ``--sysconfdir`` switch of ./configure has been invoked). Then  $(HOME)/.gdal/gdalrc
   is tried, potentially overriding what was loaded with the sysconfdir

 - for Windows builds, the location pointed by $(USERPROFILE)/.gdal/gdalrc
   is attempted.

A configuration file is a text file in a .ini style format.
Lines starting with `#` are comment lines.

The file may contain a ``[configoptions]`` section, that lists configuration
options and their values.

Example:

.. code-block::

    [configoptions]
    # set BAR as the value of configuration option FOO
    FOO=BAR


Configuration options set in the configuration file can later be overridden
by calls to :cpp:func:`CPLSetConfigOption` or  :cpp:func:`CPLSetThreadLocalConfigOption`,
or through the ``--config`` command line switch.

The value of environment variables set before GDAL starts will be used instead
of the value set in the configuration files, unless, starting with GDAL 3.6,
the configuration file starts with a ``[directives]`` section that contains a
``ignore-env-variables=yes`` entry.

.. code-block::

    [directives]
    # ignore environment variables. Take only into account the content of the
    # [configoptions] section, or ones defined programmatically with
    # CPLSetConfigOption / CPLSetThreadLocalConfigOption.
    ignore-env-variables=yes


Starting with GDAL 3.5, a configuration file can also contain credentials
(or more generally options related to a virtual file system) for a given path prefix,
that can also be set with :cpp:func:`VSISetPathSpecificOption`. Credentials should be put under
a ``[credentials]`` section, and for each path prefix, under a relative subsection
whose name starts with "[." (e.g. "[.some_arbitrary_name]"), and whose first
key is "path".

Example:

.. code-block::

    [credentials]

    [.private_bucket]
    path=/vsis3/my_private_bucket
    AWS_SECRET_ACCESS_KEY=...
    AWS_ACCESS_KEY_ID=...

    [.sentinel_s2_l1c]
    path=/vsis3/sentinel-s2-l1c
    AWS_REQUEST_PAYER=requester
    \endverbatim



Global configuration options
----------------------------

Logging
^^^^^^^

-  .. config:: CPL_CURL_VERBOSE
      :choices: YES, NO

      Set to "YES" to get the curl library to display a lot of verbose information
      about its operations. Very useful for libcurl and/or protocol debugging and
      understanding.

-  .. config:: CPL_DEBUG
      :choices: ON, OFF, <PREFIX>

      This may be set to ON, OFF or specific prefixes. If it is ON, all debug
      messages are reported to stdout. If it is OFF or unset no debug messages are
      reported. If it is set to a particular value, then only debug messages with
      that "type" value will be reported. For instance debug messages from the HFA
      driver are normally reported with type "HFA" (seen in the message).

      At the commandline this can also be set with --debug <value> as well as with
      --config CPL_DEBUG <value>.

-  .. config:: CPL_LOG
      :choices: <path>

      This is used for setting the log file path.

-  .. config:: CPL_LOG_ERRORS
      :choices: ON, OFF

      Set to "ON" for printing error messages. Use together with "CPL_LOG" for
      directing them into a file.

-  .. config:: CPL_TIMESTAMP
      :choices: ON, OFF

      Set to "ON" to add timestamps to CPL debug messages (so assumes that
      :config:`CPL_DEBUG` is enabled)

-  .. config:: CPL_MAX_ERROR_REPORTS

-  .. config:: CPL_ACCUM_ERROR_MSG



Performance and caching
^^^^^^^^^^^^^^^^^^^^^^^

-  .. config:: GDAL_NUM_THREADS
      :choices: ALL_CPUS, <integer>

      Sets the number of worker threads to be used by GDAL operations that support
      multithreading. The default value depends on the context in which it is used.

-  .. config:: GDAL_CACHEMAX
      :choices: <size>
      :default: 5%

      Controls the default GDAL raster block cache size. When
      blocks are read from disk, or written to disk, they are cached in a
      global block cache by the :cpp:class:`GDALRasterBlock` class. Once this
      cache exceeds :config:`GDAL_CACHEMAX` old blocks are flushed from the
      cache.
      This cache is mostly beneficial when needing to read or write blocks
      several times. This could occur, for instance, in a scanline oriented
      input file which is processed in multiple rectangular chunks by
      :program:`gdalwarp`.
      If its value is small (less than 100000), it is assumed to be measured in megabytes,
      otherwise in bytes. Alternatively, the value can be set to "X%" to mean X%
      of the usable physical RAM. Note that this value is only consulted the first
      time the cache size is requested.  To change this value programmatically
      during operation of the program it is better to use
      :cpp:func:`GDALSetCacheMax` (always in bytes) or or
      :cpp:func:`GDALSetCacheMax64`. The maximum practical value on 32 bit OS is
      between 2 and 4 GB. It is the responsibility of the user to set a consistent
      value.

-  .. config:: GDAL_FORCE_CACHING
      :choices: YES, NO
      :default: NO

      When set to YES, :cpp:func:`GDALDataset::RasterIO` and :cpp:func:`GDALRasterBand::RasterIO`
      will use cached IO (access block by block through
      :cpp:func:`GDALRasterBand::IReadBlock` API) instead of a potential driver-specific
      implementation of IRasterIO(). This will only have an effect on drivers that
      specialize IRasterIO() at the dataset or raster band level, for example
      JP2KAK, NITF, HFA, WCS, ECW, MrSID, and JPEG.

-  .. config:: GDAL_BAND_BLOCK_CACHE
      :choices: AUTO, ARRAY, HASHSET
      :default: AUTO

      Controls whether the block cache should be backed by an array or a hashset.
      By default (``AUTO``) the implementation will be selected based on the
      number of blocks in the dataset. See :ref:`rfc-26` for more information.

-  .. config:: GDAL_MAX_DATASET_POOL_SIZE
      :default: 100

      Used by :source_file:`gcore/gdalproxypool.cpp`

      Number of datasets that can be opened simultaneously by the GDALProxyPool
      mechanism (used by VRT for example). Can be increased to get better random I/O
      performance with VRT mosaics made of numerous underlying raster files. Be
      careful: on Linux systems, the number of file handles that can be opened by a
      process is generally limited to 1024. This is currently clamped between 2 and
      1000.

-  .. config:: GDAL_MAX_DATASET_POOL_RAM_USAGE
      :since: 3.7

      Limit the RAM usage of opened datasets in the GDALProxyPool.

      The value can also be suffixed with ``MB`` or ``GB`` to
      respectively express it in megabytes or gigabytes. The default value is 25%
      of the usable physical RAM minus the :config:`GDAL_CACHEMAX` value.

-  .. config:: GDAL_SWATH_SIZE
      :default: 1/4 of the maximum block cache size (``GDAL_CACHEMAX``)

      Used by :source_file:`gcore/rasterio.cpp`

      Size of the swath when copying raster data from one dataset to another one (in
      bytes). Should not be smaller than :config:`GDAL_CACHEMAX`.

-  .. config:: GDAL_DISABLE_READDIR_ON_OPEN
      :choices: TRUE, FALSE, EMPTY_DIR
      :default: FALSE

      By default (FALSE), GDAL establishes a list of all the files in the
      directory of the file passed to :cpp:func:`GDALOpen`. This can result in
      speed-ups in some use cases, but also to major slow downswhen the
      directory contains thousands of other files. When set to TRUE, GDAL will
      not try to establish the list of files. The number of files read can
      also be limited by :config:`GDAL_READDIR_LIMIT_ON_OPEN`.

      If set to EMPTY_DIR, only the file that is being opened will be seen when a
      GDAL driver will request sibling files, so this is a way to disable loading
      side-car/auxiliary files.

-  .. config:: GDAL_READDIR_LIMIT_ON_OPEN
      :default: 1000
      :since: 2.1

      Sets the maximum number of files to scan when searching for sidecar files
      in :cpp:func:`GDALOpen`.

-  .. config:: VSI_CACHE
      :choices: TRUE, FALSE
      :since: 1.10

      When using the VSI interface files can be cached in
      RAM by setting the configuration option ``VSI_CACHE`` to ``TRUE``. The cache size
      defaults to 25 MB, but can be modified by setting the configuration option
      :config:`VSI_CACHE_SIZE`. (in bytes).

      When enabled, this cache is used for most I/O in GDAL, including local files.

-  .. config:: VSI_CACHE_SIZE
      :choices: <size in bytes>
      :since: 1.10

      Set the size of the VSI cache. Be wary of large values for
      ``VSI_CACHE_SIZE`` when opening VRT datasources containing many source
      rasters, as this is a per-file cache.

Driver management
^^^^^^^^^^^^^^^^^

-  .. config:: GDAL_SKIP
      :choices: space-separated list

      Used by :cpp:func:`GDALDriverManager::AutoSkipDrivers`

      This option can be used to unregister one or several GDAL drivers. This can
      be useful when a driver tries to open a dataset that it should not
      recognize, or when several drivers are built-in that can open the same
      datasets (for example JP2MrSID, JP2ECW, JPEG2000 and JP2KAK for JPEG2000
      datasets). The value of this option must be a space delimited list of the
      short name of the GDAL drivers to unregister.

      This option must be set before calling :cpp:func:`GDALAllRegister`, or an
      explicit call to :cpp:func:`GDALDriverManager::AutoSkipDrivers` will be
      required.

-  .. config:: OGR_SKIP
      :choices: comma-separated list

      This option can be used to unregister one or several OGR drivers. This can be
      useful when a driver tries to open a datasource that it should not recognize, or
      when several drivers are built-in that can open the same datasets (for example
      KML, LIBKML datasources). The value of this option must be a comma delimited
      list of the short name of the OGR drivers to unregister.

-  .. config:: GDAL_DRIVER_PATH

      Used by :cpp:func:`GDALDriverManager::AutoLoadDrivers`.

      This function will automatically load drivers from shared libraries. It
      searches the "driver path" for .so (or .dll) files that start with the prefix
      "gdal_X.so". It then tries to load them and then tries to call a function
      within them called GDALRegister_X() where the 'X' is the same as the
      remainder of the shared library basename ('X' is case sensitive), or failing
      that to call GDALRegisterMe().

      There are a few rules for the driver path. If the ``GDAL_DRIVER_PATH``
      environment variable it set, it is taken to be a list of directories to
      search separated by colons on UNIX, or semi-colons on Windows. Otherwise the
      /usr/local/lib/gdalplugins directory, and (if known) the lib/gdalplugins
      subdirectory of the gdal home directory are searched on UNIX and
      $(BINDIR)\gdalplugins on Windows.

      Auto loading can be completely disabled by setting the
      ``GDAL_DRIVER_PATH`` config option to "disable".

      This option must be set before calling :cpp:func:`GDALAllRegister`, or an explicit call
      to :cpp:func:`GDALDriverManager::AutoLoadDrivers` will be required.

-  .. config:: GDAL_PYTHON_DRIVER_PATH

      A list of directories to search for ``.py`` files implementing GDAL drivers.
      Like :config:`GDAL_DRIVER_PATH`, directory names should be separated by colons
      on Unix or semi-colons on Windows. For more information, see :ref:`rfc-76`.

General options
^^^^^^^^^^^^^^^

-  .. config:: GDAL_DATA
      :choices: <path>

      Path to directory containing various GDAL data files (EPSG CSV files, S-57
      definition files, DXF header and footer files, ...).

      This option is read by the GDAL and OGR driver registration functions. It is
      used to expand EPSG codes into their description in the OSR model (WKT
      based).

      On some builds (Unix), the value can be hard-coded at compilation time to
      point to the path after installation (/usr/share/gdal/data for example). On
      Windows platform, this option must be generally declared.

-  .. config:: GDAL_CONFIG_FILE
      :since: 3.3

      The location of the GDAL config file (see :ref:`gdal_configuration_file`).

-  .. config:: CPL_TMPDIR
      :choices: <dirname>

      By default, temporary files are written into current working directory.
      Sometimes this is not optimal and it would be better to write temporary files
      on bigger or faster drives (SSD).

-  .. config:: GDAL_RASTERIO_RESAMPLING
      :choices: NEAR, BILINEAR, CUBIC, CUBICSPLINE, LANCZOS, AVERAGE, RMS, MODE, GAUSS
      :default: NEAR

      Sets the resampling algorithm to be used when reading from a raster
      into a buffer with different dimensions from the source region.

-  .. config:: CPL_VSIL_ZIP_ALLOWED_EXTENSIONS
      :choices: <comma-separated list>

      Add to zip FS handler default extensions array (zip, kmz, dwf, ods, xlsx)
      additional extensions listed in :config:`CPL_VSIL_ZIP_ALLOWED_EXTENSIONS` config
      option.

-  .. config:: CPL_VSIL_DEFLATE_CHUNK_SIZE
      :default: 1 M

-  .. config:: GDAL_DISABLE_CPLLOCALEC
      :choices: YES, NO
      :default: NO

      If set to YES (default is NO) this option will disable the normal behavior of
      the CPLLocaleC class which forces the numeric locale to "C" for selected chunks
      of code using the setlocale() call. Behavior of setlocale() in multi-threaded
      applications may be undependable but use of this option may result in problem
      formatting and interpreting numbers properly.

-  .. config:: GDAL_FILENAME_IS_UTF8
      :choices: YES, NO
      :default: YES

      This option only has an effect on Windows systems (using
      cpl_vsil_win32.cpp). If set to "NO" then filenames passed
      to functions like :cpp:func:`VSIFOpenL` will be passed on directly to CreateFile()
      instead of being converted from UTF-8 to wchar_t and passed to
      CreateFileW(). This effectively restores the pre-GDAL1.8 behavior for
      handling filenames on Windows and might be appropriate for applications that
      treat filenames as being in the local encoding.

-  .. config:: GDAL_MAX_BAND_COUNT
      :choices: <integer>
      :default: 65536

      Defines the maximum number of bands to read from a single dataset.

-  .. config:: GDAL_XML_VALIDATION
      :choices: YES, NO
      :default: YES

      Determines whether XML content should be validated against an XSD, with
      non-conformities reported as warnings.

-  .. config:: GDAL_GEOREF_SOURCES
      :since: 2.2

      Determines the order in which potential georeferencing sources are
      scanned.  Value should be a comma-separated list of sources in order of
      decreasing priority. The set of sources recognized by this option is
      driver-specific.

-  .. config:: GDAL_OVR_PROPAGATE_NODATA
      :choices: YES, NO
      :default: NO
      :since: 2.2

      When computing the value of an overview pixel, determines whether a
      single NODATA value should cause the overview pixel to be set to NODATA
      (``YES``), or whether the NODATA values should be simply ignored
      (``NO``).  This configuration option is not supported for all resampling
      algorithms/data types.


-  .. config:: USE_RRD
      :choices: YES, NO
      :default: NO

      Used by :source_file:`gcore/gdaldefaultoverviews.cpp`

      Can be set to YES to use Erdas Imagine format (.aux) as overview format. See
      :program:`gdaladdo` documentation.

-  .. config:: PYTHONSO

      Location of Python shared library file, e.g. ``pythonX.Y[...].so/.dll``.


.. _configoptions_vector:

Vector related options
^^^^^^^^^^^^^^^^^^^^^^

-  .. config:: OGR_ARC_STEPSIZE
      :choices: <degrees>
      :default: 4
      :since: 1.8.0

      Used by :cpp:func:`OGR_G_CreateFromGML` (for gml:Arc and gml:Circle) and
      :cpp:func:`OGRGeometryFactory::approximateArcAngles` to stroke arc to linestrings.

      The approximation of arcs as linestrings is done by splitting the arcs into
      subarcs of no more than the angle specified by this option.

-  .. config:: OGR_ARC_MAX_GAP
      :default: 0

      Arcs will be approximated while enforcing a maximum distance
      between adjacent points on the interpolated curve. Setting this option
      to 0 (the default) means no maximum distance applies.

-  .. config:: OGR_STROKE_CURVE
      :choices: TRUE, FALSE
      :default: FALSE

      Controls whether curved geometries should be approximated by linear geometries.

- .. config:: OGR_ORGANIZE_POLYGONS
     :choices: DEFAULT, SKIP, ONLY_CCW, CCW_INNER_JUST_AFTER_CW_OUTER

     Defines the method used to classify polygon rings as holes or shells.
     Although one of the options is named ``DEFAULT``, some drivers may default
     to a different method to reduce processing by taking advantage of a
     format's constraints. The following methods are available, in order of
     decreasing expected runtime:

     - ``DEFAULT``: perform a full analysis of the topological relationships
       between all rings, classifying them as shells or holes and associating
       them according to the OGC Simple Features convention. If the topological
       analysis determines that a valid geometry cannot be constructed, the
       result will be the same as with :config:`OGR_ORGANIZE_POLYGONS=SKIP`.

     - ``ONLY_CCW``: assume that rings with clockwise orientation represent
       shells and rings with counterclockwise orientation represent holes.
       Perform a limited topological analysis to determine which shell contains
       each hole. The Shapefile driver defaults to this method.

     - ``CCW_INNER_JUST_AFTER_CW_OUTER``: assume that rings with clockwise
       orientation represent shells and rings with counterclockwise orientation
       represent holes and immediately follow the outer ring with which they are
       associated.

     - ``SKIP``: avoid attempting to classify rings as shells or holes. A
       single geometry (Polygon/MultiPolygon/CurvePolygon/MultiSurface) will be
       returned with all polygons as top-level polygons. If non-polygonal elements
       are present, a GeometryCollection will be returned.


-  .. config:: OGR_SQL_LIKE_AS_ILIKE
      :choices: YES, NO
      :default: NO
      :since: 3.1

      If ``YES``, the LIKE operator in the OGR SQL dialect will be case-insensitive (ILIKE), as was the case for GDAL versions prior to 3.1.

-  .. config:: OGR_FORCE_ASCII
      :choices: YES, NO
      :default: YES

      Used by :cpp:func:`OGRGetXML_UTF8_EscapedString` function and by GPX, KML,
      GeoRSS and GML drivers.

      Those XML based drivers should write UTF8 content. If they are provided with non
      UTF8 content, they will replace each non-ASCII character by '?' when
      OGR_FORCE_ASCII=YES.

      Set to NO to preserve the content, but beware that the resulting XML file will
      not be valid and will require manual edition of the encoding in the XML header.

-  .. config:: OGR_APPLY_GEOM_SET_PRECISION
      :choices: YES, NO
      :default: NO
      :since: 3.9

      By default, when a geometry coordinate precision is set on a geometry field
      definition and a driver honors the GDAL_DCAP_HONOR_GEOM_COORDINATE_PRECISION
      capability, geometries passed to :cpp:func:`OGRLayer::CreateFeature` and
      :cpp:func:`OGRLayer::SetFeature` are assumed to be compatible of the
      coordinate precision. That is they are assumed to be valid once their
      coordinates are rounded to it. If it might not be the case, set this
      configuration option to YES before calling CreateFeature() or SetFeature()
      to force :cpp:func:`OGRGeometry::SetPrecision` to be called on the passed geometries.


Networking options
^^^^^^^^^^^^^^^^^^

-  .. config:: CPL_VSIL_CURL_ALLOWED_EXTENSIONS
      :choices: <comma-separated list>

      Consider that only the files whose extension ends up with one that is listed
      in :config:`CPL_VSIL_CURL_ALLOWED_EXTENSIONS` exist on the server. This can speed up
      dramatically open experience, in case the server cannot return a file list.

      For example:

      .. code-block::

         gdalinfo --config CPL_VSIL_CURL_ALLOWED_EXTENSIONS ".tif" /vsicurl/http://igskmncngs506.cr.usgs.gov/gmted/Global_tiles_GMTED/075darcsec/bln/W030/30N030W_20101117_gmted_bln075.tif

-  .. config:: CPL_VSIL_CURL_CACHE_SIZE
      :choices: <bytes>
      :default: 16 MB
      :since: 2.3

      Size of global least-recently-used (LRU) cache shared among all downloaded
      content.

-  .. config:: CPL_VSIL_CURL_USE_HEAD
      :choices: YES, NO
      :default: YES

      Controls whether to use a HEAD request when opening a remote URL.

-  .. config:: CPL_VSIL_CURL_USE_S3_REDIRECT
      :choices: YES, NO
      :default: YES
      :since: 2.1

      Try to query quietly redirected URLs to Amazon S3 signed URLs during their
      validity period, so as to minimize round-trips.

-  .. config:: CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE
      :choices: YES, NO

      Use a local temporary file to support random writes in certain virtual
      file systems. The temporary file will be located in :config:`CPL_TMPDIR`.

-  .. config:: CURL_CA_BUNDLE
      :since: 2.1.3

      Set the path to the Certification Authority (CA) bundle file.

-  .. config:: SSL_CERT_FILE
      :since: 2.1.3

-  .. config:: CPL_VSIL_CURL_CHUNK_SIZE
      :choices: <bytes>
      :since: 2.3

-  .. config:: GDAL_INGESTED_BYTES_AT_OPEN
      :since: 2.3

      Sets the number of bytes read in one GET call at file opening.

-  .. config:: CPL_VSIL_CURL_NON_CACHED
      :choices: <colon-separated list>
      :since: 2.3

      A global LRU cache of 16 MB shared among all downloaded content is enabled
      by default, and content in it may be reused after a file handle has been
      closed and reopened. The :config:`CPL_VSIL_CURL_NON_CACHED` configuration option
      can be set to values like
      ``/vsis3/bucket/foo.tif:/vsis3/another_bucket/some_directory``, so that at
      file handle closing, all cached content related to the mentioned file(s) is
      no longer cached. This can help when dealing with resources that can be
      modified during execution of GDAL-related code.

-  .. config:: GDAL_HTTP_HEADER_FILE
      :choices: <filename>
      :since: 2.3

      Filename of a text file with "key: value" HTTP headers. The content of the
      file is not cached, and thus it is read again before issuing each HTTP request.

-  .. config:: GDAL_HTTP_CONNECTTIMEOUT
      :choices: <seconds>
      :since: 2.2

      Maximum delay for connection to be established before being aborted.

-  .. config:: GDAL_HTTP_COOKIE
      :since: 2.0

      Cookie(s) to send. See https://curl.se/libcurl/c/CURLOPT_COOKIE.html

-  .. config:: GDAL_HTTP_COOKIEFILE
      :since: 2.4.0

      File name to read cookies from. See https://curl.se/libcurl/c/CURLOPT_COOKIEFILE.html

-  .. config:: GDAL_HTTP_COOKIEJAR
      :since: 2.4.0

      File to which cookies should be written. See https://curl.se/libcurl/c/CURLOPT_COOKIEJAR.html

-  .. config:: GDAL_HTTP_NETRC
      :choices: YES, NO
      :default: YES
      :since: 1.11.0

      Controls if an available ``.netrc`` file is used.

-  .. config:: GDAL_HTTP_NETRC_FILE
      :choices: <filename>
      :since: 3.7.0

      Sets the location of a ``.netrc`` file.

-  .. config:: GDAL_HTTP_LOW_SPEED_LIMIT
      :choices: <bytes/s>
      :default: 0
      :since: 2.1.0

      Sets the transfer speed, averaged over :config:`GDAL_HTTP_LOW_SPEED_TIME`, below which a request should be canceled.

-  .. config:: GDAL_HTTP_LOW_SPEED_TIME
      :choices: <seconds>
      :default: 0
      :since: 2.1.0

      Sets the time window over which :config:`GDAL_HTTP_LOW_SPEED_LIMIT` should be evaluated.

-  .. config:: GDAL_HTTP_SSL_VERIFYSTATUS
      :choices: YES, NO
      :default: NO
      :since: 2.3.0

      Whether to verify the status of SSL certificates. See https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYSTATUS.html

-  .. config:: GDAL_HTTP_USE_CAPI_STORE
      :choices: YES, NO
      :default: NO
      :since: 2.3.0

      (Windows only). Whether to use certificates from the Windows certificate store.

-  .. config:: GDAL_HTTP_HEADERS
      :since: 3.6

      Specifies headers as a comma separated list of key: value pairs. If a comma
      or a double-quote character is needed in the value, then the key: value pair
      must be enclosed in double-quote characters. In that situation, backslash
      and double quote character must be backslash-escaped.  e.g
      GDAL_HTTP_HEADERS=Foo: Bar,"Baz: escaped backslash \\\\, escaped double-quote
      \\", end of value",Another: Header

-  .. config:: GDAL_HTTP_MAX_RETRY
      :since: 2.3
      :default: 0

      Set the number of HTTP attempts, when a retry is allowed.
      (cf :config:`GDAL_HTTP_RETRY_CODES` for conditions where a retry is attempted.)
      The default value is 0, meaning no retry.

-  .. config:: GDAL_HTTP_RETRY_DELAY
      :choices: <seconds>
      :since: 2.3
      :default: 30

      Set the delay between HTTP attempts.

-  .. config:: GDAL_HTTP_RETRY_CODES
      :choices: ALL or comma-separated list of codes
      :since: 3.10

      Specify which HTTP error codes should trigger a retry attempt.
      Valid values are ``ALL`` or a comma-separated list of HTTP codes.
      By default, 429, 500, 502, 503 or 504 HTTP errors are considered, as
      well as other situations with a particular HTTP or Curl error message.

-  .. config:: GDAL_HTTP_TCP_KEEPALIVE
      :choices: YES, NO
      :default: NO
      :since: 3.6

      Sets whether to enable TCP keep-alive.

-  .. config:: GDAL_HTTP_TCP_KEEPIDLE
      :choices: <seconds>
      :default: 60
      :since: 3.6

      Keep-alive idle time. Only taken into account if
      :config:`GDAL_HTTP_TCP_KEEPALIVE=YES`.

-  .. config:: GDAL_HTTP_TCP_KEEPINTVL
      :choices: <seconds>
      :default: 60
      :since: 3.6

      Interval time between keep-alive probes. Only taken into account if
      :config:`GDAL_HTTP_TCP_KEEPALIVE=YES`.

-  .. config:: GDAL_HTTP_SSLCERT
      :choices: <filename>
      :since: 3.7

      Filename of the the SSL client certificate. See https://curl.se/libcurl/c/CURLOPT_SSLCERT.html

-  .. config:: GDAL_HTTP_SSLCERTTYPE
      :choices: PEM, DER
      :since: 3.7

      Format of the SSL certificate. see
      https://curl.se/libcurl/c/CURLOPT_SSLCERTTYPE.html

-  .. config:: GDAL_HTTP_SSLKEY
      :choices: <filename>
      :since: 3.7

      Private key file for TLS and SSL client certificate. see
      https://curl.se/libcurl/c/CURLOPT_SSLKEY.html

-  .. config:: GDAL_HTTP_KEYPASSWD
      :since: 3.7

      Passphrase to private key. See https://curl.se/libcurl/c/CURLOPT_KEYPASSWD.html

-  .. config:: GDAL_HTTP_VERSION
      :since: 2.3
      :choices: 1.0, 1.1, 2, 2TLS

      Specifies which HTTP version to use. Will default to 1.1 generally (except on
      some controlled environments, like Google Compute Engine VMs, where 2TLS will
      be the default). Support for HTTP/2 requires curl 7.33 or later, built
      against nghttp2. "2TLS" means that HTTP/2 will be attempted for HTTPS
      connections only. Whereas "2" means that HTTP/2 will be attempted for HTTP or
      HTTPS. The interest of enabling HTTP/2 is the use of HTTP/2 multiplexing when
      reading GeoTIFFs stored on /vsicurl/ and related virtual file systems.

-  .. config:: GDAL_HTTP_MULTIPLEX
      :since: 2.3
      :choices: YES, NO

      Defaults to YES. Only applies on a HTTP/2 connection. If set to YES, HTTP/2
      multiplexing can be used to download multiple ranges in parallel, during
      ReadMultiRange() requests that can be emitted by the GeoTIFF driver.

-  .. config:: GDAL_HTTP_MULTIRANGE
      :since: 2.3
      :choices: SINGLE_GET, SERIAL, YES
      :default: YES

      Controls how ReadMultiRange() requests emitted by the GeoTIFF driver are
      satisfied. SINGLE_GET means that several ranges will be expressed in the
      Range header of a single GET requests, which is not supported by a majority
      of servers (including AWS S3 or Google GCS). SERIAL means that each range
      will be requested sequentially. YES means that each range will be requested
      in parallel, using HTTP/2 multiplexing or several HTTP connections.

-  .. config:: GDAL_HTTP_MERGE_CONSECUTIVE_RANGES
      :since: 2.3
      :choices: YES, NO
      :default: YES

      Only applies when :config:`GDAL_HTTP_MULTIRANGE` is YES. Defines if ranges
      of a single ReadMultiRange() request that are consecutive should be merged
      into a single request.

-  .. config:: GDAL_HTTP_AUTH
      :choices: BASIC, NTLM, NEGOTIATE, ANY, ANYSAFE, BEARER

      Set value to tell libcurl which authentication method(s) you want it to
      use. See http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTHTTPAUTH
      for more information.

-  .. config:: GDAL_HTTP_USERPWD

      The HTTP user and password to use for the connection. Must be in the form of
      [user name]:[password]. Use :config:`GDAL_HTTP_AUTH` to decide the
      authentication method.

      When using NTLM, you can set the domain by prepending it to the user name and
      separating the domain and name with a forward (/) or backward slash (\). Like
      this: "domain/user:password" or "domain\user:password". Some HTTP servers (on
      Windows) support this style even for Basic authentication.

-  .. config:: GDAL_GSSAPI_DELEGATION
      :since: 3.3
      :choices: NONE, POLICY, ALWAYS

      Set allowed GSS-API delegation. Relevant only with
      :config:`GDAL_HTTP_AUTH=NEGOTIATE`.

-  .. config:: GDAL_HTTP_BEARER
      :since: 3.9

      Set HTTP OAuth 2.0 Bearer Access Token to use for the connection. Must be used
      with :config:`GDAL_HTTP_AUTH=BEARER`.

-  .. config:: GDAL_HTTP_PROXY

      Set HTTP proxy to use. The parameter should be the host name or dotted IP
      address. To specify port number in this string, append :[port] to the end of the
      host name. The proxy string may be prefixed with [protocol]: since any such
      prefix will be ignored. The proxy's port number may optionally be specified with
      the separate option. If not specified, libcurl will default to using port 1080
      for proxies.

      GDAL respects the environment variables http_proxy, ftp_proxy, all_proxy etc, if
      any of those are set. GDAL_HTTP_PROXY option does however override any possibly
      set environment variables.

-  .. config:: GDAL_HTTPS_PROXY

      Set HTTPS proxy to use. See :config:`GDAL_HTTP_PROXY`.

-  .. config:: GDAL_HTTP_PROXYUSERPWD

      The HTTP user and password to use for the connection to the HTTP proxy. Must be
      in the form of [user name]:[password].

-  .. config:: GDAL_PROXY_AUTH
      :choices: BASIC, NTLM, NEGOTIATE, DIGEST, ANY, ANYSAFE

      Set value to  to tell libcurl which authentication method(s) you want it to use
      for your proxy authentication. See
      http://curl.haxx.se/libcurl/c/curl_easy_setopt.html#CURLOPTPROXYAUTH for more
      information.

-  .. config:: CPL_CURL_GZIP
      :choices: YES, NO

      Sets the contents of the Accept-Encoding header sent in a HTTP request to gzip,
      and enables decoding of a response when a Content-Encoding: header

-  .. config:: GDAL_HTTP_TIMEOUT

      Set HTTP timeout value, where value is in seconds

-  .. config:: GDAL_HTTP_USERAGENT

      This string will be used to set the ``User-Agent`` header in the HTTP
      request sent to the remote server.
      Defaults to "GDAL/x.y.z" where x.y.z is the GDAL build version.

-  .. config:: GDAL_HTTP_UNSAFESSL
      :choices: YES, NO
      :default: NO

      Set to "YES" to get the curl library to skip SSL host / certificate
      verification.


Persistent Auxiliary Metadata (PAM) options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  .. config:: GDAL_PAM_ENABLED
      :choices: YES, NO

      PAM support can be enabled (resp. disabled) in GDAL by setting the
      :config:`GDAL_PAM_ENABLED` configuration option (via CPLSetConfigOption(), or the
      environment) to the value of YES (resp. NO). Note: The default value is build
      dependent and defaults to YES in Windows and Unix builds. See :cpp:class:`GDALPamDataset`
      for more information. Note that setting this option to OFF may have
      subtle/silent side-effects on various drivers that rely on PAM functionality.

-  .. config:: GDAL_PAM_PROXY_DIR

      Directory to which ``.aux.xml`` files will be written when accessing
      files from a location where the user does not have write permissions. Has
      no effect when accessing files from locations where the user does have
      write permissions. Must be set before the first access to PAM.

PROJ options
^^^^^^^^^^^^

-  .. config:: CENTER_LONG

-  .. config:: CHECK_WITH_INVERT_PROJ
      :since: 1.7.0

      Used by :source_file:`ogr/ogrct.cpp` and :source_file:`apps/gdalwarp_lib.cpp`.

      This option can be used to control the behavior of gdalwarp when warping global
      datasets or when transforming from/to polar projections, which causes
      coordinate discontinuities. See http://trac.osgeo.org/gdal/ticket/2305.

      The background is that PROJ does not guarantee that converting from src_srs to
      dst_srs and then from dst_srs to src_srs will yield to the initial coordinates.
      This can lead to errors in the computation of the target bounding box of
      gdalwarp, or to visual artifacts.

      If CHECK_WITH_INVERT_PROJ option is not set, gdalwarp will check that the the
      computed coordinates of the edges of the target image are in the validity area
      of the target projection. If they are not, it will retry computing them by
      setting :config:`CHECK_WITH_INVERT_PROJ=TRUE` that forces ogrct.cpp to check the
      consistency of each requested projection result with the invert projection.

      If set to NO, gdalwarp will not attempt to use the invert projection.

-  .. config:: THRESHOLD
      :since: 1.7.0

      Used by :source_file:`ogr/ogrct.cpp`.

      Used in combination with :config:`CHECK_WITH_INVERT_PROJ=TRUE`. Define
      the acceptable threshold used to check if the roundtrip from src_srs to
      dst_srs and from dst_srs to srs_srs yield to the initial coordinates. The
      value must be expressed in the units of the source SRS (typically degrees
      for a geographic SRS, meters for a projected SRS)

-  .. config:: OGR_ENABLE_PARTIAL_REPROJECTION
      :since: 1.8.0
      :choices: YES, NO
      :default: NO

      Used by :cpp:func:`OGRLineString::transform`.

      Can be set to YES to remove points that cannot be reprojected. This can for example help reproject lines that have an extremity at a pole, when the reprojection does not support coordinates at poles.

-  .. config:: OGR_CT_USE_SRS_COORDINATE_EPOCH
      :choices: YES, NO

      If ``NO``, disables the coordinate epoch associated with the target or
      source CRS when transforming between a static and dynamic CRS.

-  .. config:: OSR_ADD_TOWGS84_ON_EXPORT_TO_WKT1
      :choices: YES, NO
      :default: NO
      :since: 3.0.3

      Determines whether a ``TOWGS84`` node should be automatically added when exporting
      a CRS to the GDAL flavor of WKT1.

-  .. config:: OSR_ADD_TOWGS84_ON_EXPORT_TO_PROJ4
      :choices: YES, NO
      :default: YES
      :since: 3.0.3

      Determines whether a ``+towgs84`` parameter should be automatically added when
      exporting a CRS as a legacy PROJ.4 string.

-  .. config:: OSR_ADD_TOWGS84_ON_IMPORT_FROM_EPSG
      :choices: YES, NO
      :default: NO
      :since: 3.0.3

      Determines whether to automatically add a 3-parameter or 7-parameter
      Helmert transformation to WGS84 when there is exactly one such method
      available for the CRS.

-  .. config:: OSR_DEFAULT_AXIS_MAPPING_STRATEGY
      :choices: TRADITIONAL_GIS_ORDER, AUTHORITY_COMPLIANT
      :default: AUTHORITY_COMPLIANT
      :since: 3.5

      Determines whether to honor the declared axis mapping of a CRS or override it
      with the traditional GIS ordering (x = longitude, y = latitude).

-  .. config:: OSR_STRIP_TOWGS84
      :choices: YES, NO
      :default: YES
      :since: 3.1

      Determines whether to remove TOWGS84 information if the CRS has a known
      horizontal datum.

-  .. config:: OSR_USE_NON_DEPRECATED
      :choices: YES, NO
      :default: YES

      Determines whether to substitute a replacement for deprecated EPSG codes.

-  .. config:: OSR_WKT_FORMAT
      :choices: SFSQL, WKT1_SIMPLE, WKT1, WKT1_GDAL, WKT1_ESRI, WKT2_2015, WKT2_2018, WKT2, DEFAULT
      :default: DEFAULT

      Sets the format for writing a CRS to WKT.

.. _list_config_options:

List of configuration options and where they are documented
-----------------------------------------------------------

.. config_index::
   :types: config

