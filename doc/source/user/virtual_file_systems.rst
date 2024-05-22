.. _virtual_file_systems:

===========================================================================================================
GDAL Virtual File Systems (compressed, network hosted, etc...): /vsimem, /vsizip, /vsitar, /vsicurl, ...
===========================================================================================================

Introduction
------------

GDAL can access files located on "standard" file systems, i.e. in the / hierarchy on Unix-like systems or in C:\, D:\, etc... drives on Windows. But most GDAL raster and vector drivers use a GDAL-specific abstraction to access files. This makes it possible to access less standard types of files, such as in-memory files, compressed files (.zip, .gz, .tar, .tar.gz archives), encrypted files, standard input and output (STDIN, STDOUT), files stored on network (either publicly accessible, or in private buckets of commercial cloud storage services), etc.

Each special file system has a prefix, and the general syntax to name a file is /vsiPREFIX/...

Example:

::

    gdalinfo /vsizip/my.zip/my.tif

Chaining
--------

It is possible to chain multiple file system handlers.

::

    # ogrinfo a shapefile in a zip file on the internet:

    ogrinfo -ro -al -so /vsizip//vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/ogr/data/shp/poly.zip

    # ogrinfo a shapefile in a zip file on an ftp:

    ogrinfo -ro -al -so /vsizip//vsicurl/ftp://user:password@example.com/foldername/file.zip/example.shp

(Note is also OK to say /vsizip/vsicurl/... with a single slash. (But when writing documentation please still use two.))

Drivers supporting virtual file systems
---------------------------------------

Virtual file systems can only be used with GDAL or OGR drivers supporting the "large file API", which is now the vast majority of file based drivers. The full list of these formats can be obtained by looking at the driver marked with 'v' when running either ``gdalinfo --formats`` or ``ogrinfo --formats``.

A notable exception is the HDF4 driver.

.. _vsizip:

/vsizip/ (.zip archives)
------------------------

Read capabilities
+++++++++++++++++

/vsizip/ is a file handler that allows reading ZIP archives on-the-fly without decompressing them beforehand.

To point to a file inside a zip file, the filename must be of the form :file:`/vsizip/path/to/the/file.zip/path/inside/the/zip/file`, where :file:`path/to/the/file.zip` is relative or absolute and :file:`path/inside/the/zip/file` is the relative path to the file inside the archive.

To use the .zip as a directory, you can use :file:`/vsizip/path/to/the/file.zip` or :file:`/vsizip/path/to/the/file.zip/subdir`. Directory listing is available with :cpp:func:`VSIReadDir`. A :cpp:func:`VSIStatL` ("/vsizip/...") call will return the uncompressed size of the file. Directories inside the ZIP file can be distinguished from regular files with the VSI_ISDIR(stat.st_mode) macro as for regular file systems. Getting directory listing and file statistics are fast operations.

Note: in the particular case where the .zip file contains a single file located at its root, just mentioning :file:`/vsizip/path/to/the/file.zip` will work.

The following configuration options are specific to the /zip/ handler:

-  .. config:: CPL_SOZIP_ENABLED
      :choices: YES, NO, AUTO
      :default: AUTO
      :since: 3.7

      Determines whether the SOZip optimization should be enabled. If ``AUTO``,
      SOZip will be enabled for uncompressed files larger than
      :config:`CPL_SOZIP_MIN_FILE_SIZE`.


-  .. config:: CPL_SOZIP_MIN_FILE_SIZE
      :default: 1M
      :since: 3.7

      Determines the minimum file size for SOZip to be automatically enabled.


Examples:

::

    /vsizip/my.zip/my.tif  (relative path to the .zip)
    /vsizip//home/even/my.zip/subdir/my.tif  (absolute path to the .zip)
    /vsizip/c:\users\even\my.zip\subdir\my.tif

.kmz, .ods and .xlsx extensions are also detected as valid extensions for zip-compatible archives.

Starting with GDAL 2.2, an alternate syntax is available so as to enable chaining and not being dependent on .zip extension, e.g.: ``/vsizip/{/path/to/the/archive}/path/inside/the/zip/file``. Note that :file:`/path/to/the/archive` may also itself use this alternate syntax.

Write capabilities
++++++++++++++++++

Write capabilities are also available. They allow creating a new zip file and adding new files to an already existing (or just created) zip file.

Creation of a new zip file:

::

    fmain = VSIFOpenL("/vsizip/my.zip", "wb");
    subfile = VSIFOpenL("/vsizip/my.zip/subfile", "wb");
    VSIFWriteL("Hello World", 1, strlen("Hello world"), subfile);
    VSIFCloseL(subfile);
    VSIFCloseL(fmain);

Addition of a new file to an existing zip:

::

    newfile = VSIFOpenL("/vsizip/my.zip/newfile", "wb");
    VSIFWriteL("Hello World", 1, strlen("Hello world"), newfile);
    VSIFCloseL(newfile);

Starting with GDAL 2.4, the :config:`GDAL_NUM_THREADS` configuration option can be set to an integer or ``ALL_CPUS`` to enable multi-threaded compression of a single file. This is similar to the pigz utility in independent mode. By default the input stream is split into 1 MB chunks (the chunk size can be tuned with the :config:`CPL_VSIL_DEFLATE_CHUNK_SIZE` configuration option, with values like "x K" or "x M"), and each chunk is independently compressed (and terminated by a nine byte marker 0x00 0x00 0xFF 0xFF 0x00 0x00 0x00 0xFF 0xFF, signaling a full flush of the stream and dictionary, enabling potential independent decoding of each chunk). This slightly reduces the compression rate, so very small chunk sizes should be avoided.
Starting with GDAL 3.7, this technique is reused to generate .zip files following :ref:`sozip_intro`.

Read and write operations cannot be interleaved. The new zip must be closed before being re-opened in read mode.

.. _sozip_intro:

SOZip (Seek-Optimized ZIP)
++++++++++++++++++++++++++

GDAL (>= 3.7) has full read and write support for .zip files following the
`SOZip (Seek-Optimized ZIP) <https://sozip.org>`__ profile.

* The ``/vsizip/`` virtual file system uses the SOZip index to perform fast
  random access within a compressed SOZip-enabled file.

* The :ref:`vector.shapefile` and :ref:`vector.gpkg` drivers can directly generate
  SOZip-enabled .shz/.shp.zip or .gpkg.zip files.

* The :cpp:func:`CPLAddFileInZip` C function, which can compress a file and add
  it to an new or existing ZIP file, enables the SOZip optimization when relevant
  (ie when a file to be compressed is larger than 1 MB).
  SOZip optimization can be forced by setting the :config:`CPL_SOZIP_ENABLED`
  configuration option to YES. Or totally disabled by setting it to NO.

* The :cpp:func:`VSIGetFileMetadata` method can be called on a filename of
  the form :file:`/vsizip/path/to/the/file.zip/path/inside/the/zip/file` and
  with domain = "ZIP" to get information if a SOZip index is available for that file.

* The :ref:`sozip` new command line utility can be used to create a
  seek-optimized ZIP file, to append files to an existing ZIP file, list the
  contents of a ZIP file and display the SOZip optimization status or validate a SOZip file.


.. _vsigzip:

/vsigzip/ (gzipped file)
------------------------

/vsigzip/ is a file handler that allows on-the-fly reading of GZip (.gz) files without decompressing them in advance.

To view a gzipped file as uncompressed by GDAL, you must use the :file:`/vsigzip/path/to/the/file.gz` syntax, where :file:`path/to/the/file.gz` is relative or absolute.

The following configuration options are specific to the /vsigzip/ handler:

-  .. config:: CPL_VSIL_GZIP_WRITE_PROPERTIES
      :choices: YES, NO
      :default: YES

      If ``YES``, when the file is located in a writable location, a file with
      extension .gz.properties is created with an indication of the
      uncompressed file size.


Examples:

::

    /vsigzip/my.gz # (relative path to the .gz)
    /vsigzip//home/even/my.gz # (absolute path to the .gz)
    /vsigzip/c:\users\even\my.gz

:cpp:func:`VSIStatL` will return the uncompressed file size, but this is potentially a slow operation on large files, since it requires uncompressing the whole file. Seeking to the end of the file, or at random locations, is similarly slow. To speed up that process, "snapshots" are internally created in memory so as to be able being able to seek to part of the files already decompressed in a faster way. This mechanism of snapshots also apply to /vsizip/ files.

Write capabilities are also available, but read and write operations cannot be interleaved.

Starting with GDAL 2.4, the :config:`GDAL_NUM_THREADS` configuration option can be set to an integer or ``ALL_CPUS`` to enable multi-threaded compression of a single file. This is similar to the pigz utility in independent mode. By default the input stream is split into 1 MB chunks (the chunk size can be tuned with the :config:`CPL_VSIL_DEFLATE_CHUNK_SIZE` configuration option, with values like "x K" or "x M"), and each chunk is independently compressed (and terminated by a nine byte marker 0x00 0x00 0xFF 0xFF 0x00 0x00 0x00 0xFF 0xFF, signaling a full flush of the stream and dictionary, enabling potential independent decoding of each chunk). This slightly reduces the compression rate, so very small chunk sizes should be avoided.

.. _vsitar:

/vsitar/ (.tar, .tgz archives)
------------------------------

/vsitar/ is a file handler that allows on-the-fly reading in regular uncompressed .tar or compressed .tgz or .tar.gz archives, without decompressing them in advance.

To point to a file inside a .tar, .tgz .tar.gz file, the filename must be of the form :file:`/vsitar/path/to/the/file.tar/path/inside/the/tar/file`, where :file:`path/to/the/file.tar` is relative or absolute and :file:`path/inside/the/tar/file` is the relative path to the file inside the archive.

To use the .tar as a directory, you can use :file:`/vsizip/path/to/the/file.tar` or :file:`/vsitar/path/to/the/file.tar/subdir`. Directory listing is available with :cpp:func:`VSIReadDir`. A :cpp:func:`VSIStatL` ("/vsitar/...") call will return the uncompressed size of the file. Directories inside the TAR file can be distinguished from regular files with the VSI_ISDIR(stat.st_mode) macro as for regular file systems. Getting directory listing and file statistics are fast operations.

Note: in the particular case where the .tar file contains a single file located at its root, just mentioning :file:`/vsitar/path/to/the/file.tar` will work.

Examples:

::

    /vsitar/my.tar/my.tif # (relative path to the .tar)
    /vsitar//home/even/my.tar/subdir/my.tif # (absolute path to the .tar)
    /vsitar/c:\users\even\my.tar\subdir\my.tif

Starting with GDAL 2.2, an alternate syntax is available so as to enable chaining and not being dependent on .tar extension, e.g.: :file:`/vsitar/{/path/to/the/archive}/path/inside/the/tar/file`. Note that :file:`/path/to/the/archive` may also itself use this alternate syntax.

.. _vsi7z:

/vsi7z/ (.7z archives)
----------------------

.. versionadded:: 3.7

/vsi7z/ is a file handler that allows reading `7z <https://en.wikipedia.org/wiki/7z>`__
archives on-the-fly without decompressing them beforehand. This file system is
read-only. Directory listing and :cpp:func:`VSIStatL` are available, similarly
to above mentioned file systems.

It requires GDAL to be built against `libarchive <https://libarchive.org/>`__
(and libarchive having LZMA support to be of practical use).

To point to a file inside a 7z file, the filename must be of the form
:file:`/vsi7z/path/to/the/file.7z/path/inside/the/7z/file`, where
:file:`path/to/the/file.7z` is relative or absolute and :file:`path/inside/the/7z/file`
is the relative path to the file inside the archive.`

Default extensions recognized by this virtual file system are:
``7z``, ``lpk`` (Esri ArcGIS Layer Package), ``lpkx``, ``mpk`` (Esri ArcGIS Map Package),
``mpkx`` and ``ppkx`` (Esri ArcGIS Pro Project Package).

An alternate syntax is available so as to enable chaining and not being
dependent on those extensions, e.g.: :file:`/vsi7z/{/path/to/the/archive}/path/inside/the/archive`.
Note that :file:`/path/to/the/archive` may also itself use this alternate syntax.

Note that random seeking within a large compressed file will be inefficient when
backward seeking is needed (decompression will be restarted from the start of the
file). Performance will be the best in sequential reading.

.. _vsirar:

/vsirar/ (.rar archives)
------------------------

.. versionadded:: 3.7

/vsirar/ is a file handler that allows reading `RAR <https://en.wikipedia.org/wiki/RAR_(file_format)>`__
archives on-the-fly without decompressing them beforehand. This file system is
read-only. Directory listing and :cpp:func:`VSIStatL` are available, similarly
to above mentioned file systems.

It requires GDAL to be built against `libarchive <https://libarchive.org/>`__
(and libarchive having LZMA support to be of practical use).

To point to a file inside a RAR file, the filename must be of the form
:file:`/vsirar/path/to/the/file.rar/path/inside/the/rar/file`, where
:file:`path/to/the/file.rar` is relative or absolute and :file:`path/inside/the/rar/file`
is the relative path to the file inside the archive.`

The default extension recognized by this virtual file system is: ``rar``

An alternate syntax is available so as to enable chaining and not being
dependent on those extensions, e.g.: :file:`/vsirar/{/path/to/the/archive}/path/inside/the/archive`.
Note that :file:`/path/to/the/archive` may also itself use this alternate syntax.

Note that random seeking within a large compressed file will be inefficient when
backward seeking is needed (decompression will be restarted from the start of the
file). Performance will be the best in sequential reading.

.. _network_based_file_systems:

Network based file systems
--------------------------

A generic :ref:`/vsicurl/ <vsicurl>` file system handler exists for online resources that do not require particular signed authentication schemes. It is specialized into sub-filesystems for commercial cloud storage services, such as :ref:`/vsis3/ <vsis3>`,  :ref:`/vsigs/ <vsigs>`, :ref:`/vsiaz/ <vsiaz>`, :ref:`/vsioss/ <vsioss>` or  :ref:`/vsiswift/ <vsiswift>`.

When reading of entire files in a streaming way is possible, prefer using the :ref:`/vsicurl_streaming/ <vsicurl_streaming>`, and its variants for the above cloud storage services, for more efficiency.

How to set credentials ?
++++++++++++++++++++++++

Cloud storage services require setting credentials. For some of them, they can
be provided through configuration files (~/.aws/config, ~/.boto, ..) or through
environment variables / configuration options.

Starting with GDAL 3.6, :cpp:func:`VSISetPathSpecificOption` can be used to set configuration
options with a granularity at the level of a file path, which makes it easier if using
the same virtual file system but with different credentials (e.g. different
credentials for bucket "/vsis3/foo" and "/vsis3/bar")

Starting with GDAL 3.5, credentials (or path specific options) can be specified in a
:ref:`GDAL configuration file <gdal_configuration_file>`, either in a specific one
explicitly loaded with :cpp:func:`CPLLoadConfigOptionsFromFile`, or
one of the default automatically loaded by :cpp:func:`CPLLoadConfigOptionsFromPredefinedFiles`.

They should be put under a ``[credentials]`` section, and for each path prefix,
under a relative subsection whose name starts with ``[.`` (e.g. ``[.some_arbitrary_name]``),
and whose first key is ``path``.
`
.. code-block::

    [credentials]

    [.private_bucket]
    path=/vsis3/my_private_bucket
    AWS_SECRET_ACCESS_KEY=...
    AWS_ACCESS_KEY_ID=...

    [.sentinel_s2_l1c]
    path=/vsis3/sentinel-s2-l1c
    AWS_REQUEST_PAYER=requester

Network/cloud-friendliness and file formats
+++++++++++++++++++++++++++++++++++++++++++

While most GDAL raster and vector file systems can be accessed in a remote way
with /vsicurl/ and other derived virtual file systems, performance is highly
dependent on the format, and even for a given format on the special data
arrangement. Performance also depends on the particular access pattern made
to the file.

For interactive visualisation of raster files, the file should ideally have
the following characteristics:

- it should be tiled in generally square-shaped tiles.
- it should have an index of the tile location within the file
- it should have overviews/pyramids

TIFF/GeoTIFF
~~~~~~~~~~~~

Cloud-optimized GeoTIFF files as generated by the :ref:`raster.cog` driver are
suitable for network access. More generally tiled GeoTIFF files with overviews
are.

JPEG2000
~~~~~~~~

JPEG2000 is generally not suitable for network access, unless using a layout
carefully designed for that purpose, and when using a JPEG200 library that is
heavily optimized.

JPEG2000 files can come in many flavors : single-tiled vs tiled, with different
progression order (this is of particular importance for single-tiled access),
and with optional markers

The OpenJPEG library (usable through the :ref:`raster.jp2openjpeg` driver),
at the time of writing, needs to ingest each tile-part that participates
to the area of interest of the pixel query in a whole (and thus for a
single-tiled file, to ingest the whole file). It also does not make use of the
potentially present TLM (Tile-Part length) marker, which is the equivalent of a
tile index, nor PLT (Packed Length, tile-part header), which is an index of
packets within a tile. The Kakadu library  (usable through
the :ref:`raster.jp2kak` driver), can use those markers to limit the number
of bytes to ingest (but for single-tiled raster, performance might still suffer.)

The `dump_jp2.py <https://raw.githubusercontent.com/OSGeo/gdal/master/swig/python/gdal-utils/osgeo_utils/samples/dump_jp2.py>`__
Python script can be used to check the characteristics of a given JPEG200 file.
Fields of interest to examine in the output are:

- the tile size (given by the ``XTsiz`` and ``YTsiz`` fields in the ``SIZ`` marker)
- the presence of ``TLM`` markers
- the presence of ``PLT`` markers

.. _vsicurl:

/vsicurl/ (http/https/ftp files: random access)
+++++++++++++++++++++++++++++++++++++++++++++++

/vsicurl/ is a file system handler that allows on-the-fly random reading of files available through HTTP/FTP web protocols, without prior download of the entire file. It requires GDAL to be built against libcurl.

Recognized filenames are of the form :file:`/vsicurl/http[s]://path/to/remote/resource` or :file:`/vsicurl/ftp://path/to/remote/resource`, where :file:`path/to/remote/resource` is the URL of a remote resource.

Example using :program:`ogrinfo` to read a shapefile on the internet:

::

    ogrinfo -ro -al -so /vsicurl/https://raw.githubusercontent.com/OSGeo/gdal/master/autotest/ogr/data/poly.shp

Starting with GDAL 2.3, options can be passed in the filename with the following syntax: ``/vsicurl?[option_i=val_i&]*url=http://...`` where each option name and value (including the value of "url") is URL-encoded. Currently supported options are:

- use_head=yes/no: whether the HTTP HEAD request can be emitted. Default to YES. Setting this option overrides the behavior of the :config:`CPL_VSIL_CURL_USE_HEAD` configuration option.
- max_retry=number: default to 0. Setting this option overrides the behavior of the :config:`GDAL_HTTP_MAX_RETRY` configuration option.
- retry_delay=number_in_seconds: default to 30. Setting this option overrides the behavior of the :config:`GDAL_HTTP_RETRY_DELAY` configuration option.
- retry_codes=``ALL`` or comma-separated list of HTTP error codes. Setting this option overrides the behavior of the :config:`GDAL_HTTP_RETRY_CODES` configuration option. (GDAL >= 3.10)
- list_dir=yes/no: whether an attempt to read the file list of the directory where the file is located should be done. Default to YES.
- useragent=value: HTTP UserAgent header
- referer=value: HTTP Referer header
- cookie=value: HTTP Cookie header
- header_file=value: Filename that contains one or several "Header: Value" lines
- unsafessl=yes/no
- low_speed_time=value
- low_speed_limit=value
- proxy=value
- proxyauth=value
- proxyuserpwd=value
- pc_url_signing=yes/no: whether to use the URL signing mechanism of Microsoft Planetary Computer (https://planetarycomputer.microsoft.com/docs/concepts/sas/). (GDAL >= 3.5.2). Note that starting with GDAL 3.9, this may also be set with the path-specific option ( cf :cpp:func:`VSISetPathSpecificOption`) ``VSICURL_PC_URL_SIGNING`` set to ``YES``.
- pc_collection=name: name of the collection of the dataset for Planetary Computer URL signing. Only used when pc_url_signing=yes. (GDAL >= 3.5.2)

Partial downloads (requires the HTTP server to support random reading) are done with a 16 KB granularity by default. Starting with GDAL 2.3, the chunk size can be configured with the :config:`CPL_VSIL_CURL_CHUNK_SIZE` configuration option, with a value in bytes. If the driver detects sequential reading, it will progressively increase the chunk size up to 128 times :config:`CPL_VSIL_CURL_CHUNK_SIZE` (so 2 MB by default) to improve download performance.

In addition, a global least-recently-used cache of 16 MB shared among all downloaded content is used, and content in it may be reused after a file handle has been closed and reopen, during the life-time of the process or until :cpp:func:`VSICurlClearCache` is called. Starting with GDAL 2.3, the size of this global LRU cache can be modified by setting the configuration option :config:`CPL_VSIL_CURL_CACHE_SIZE` (in bytes).

When increasing the value of :config:`CPL_VSIL_CURL_CHUNK_SIZE` to optimize sequential reading, it is recommended to increase :config:`CPL_VSIL_CURL_CACHE_SIZE` as well to 128 times the value of :config:`CPL_VSIL_CURL_CHUNK_SIZE`.

Starting with GDAL 2.3, the :config:`GDAL_INGESTED_BYTES_AT_OPEN` configuration option can be set to impose the number of bytes read in one GET call at file opening (can help performance to read Cloud optimized geotiff with a large header).

The :config:`GDAL_HTTP_PROXY` (for both HTTP and HTTPS protocols), :config:`GDAL_HTTPS_PROXY` (for HTTPS protocol only), :config:`GDAL_HTTP_PROXYUSERPWD` and :config:`GDAL_PROXY_AUTH` configuration options can be used to define a proxy server. The syntax to use is the one of Curl ``CURLOPT_PROXY``, ``CURLOPT_PROXYUSERPWD`` and ``CURLOPT_PROXYAUTH`` options.

Starting with GDAL 2.1.3, the :config:`CURL_CA_BUNDLE` or :config:`SSL_CERT_FILE` configuration options can be used to set the path to the Certification Authority (CA) bundle file (if not specified, curl will use a file in a system location).

Starting with GDAL 2.3, additional HTTP headers can be sent by setting the :config:`GDAL_HTTP_HEADER_FILE` configuration option to point to a filename of a text file with "key: value" HTTP headers.

As an alternative, starting with GDAL 3.6, the
:config:`GDAL_HTTP_HEADERS` configuration option can also be
used to specify headers. :config:`CPL_CURL_VERBOSE=YES` allows one to see them and more, when combined with ``--debug``.

Starting with GDAL 2.3, the :config:`GDAL_HTTP_MAX_RETRY` (number of attempts) and :config:`GDAL_HTTP_RETRY_DELAY` (in seconds) configuration option can be set, so that request retries are done in case of HTTP errors 429, 502, 503 or 504.

Starting with GDAL 3.6, the following configuration options control the TCP keep-alive functionality (cf https://daniel.haxx.se/blog/2020/02/10/curl-ootw-keepalive-time/ for a detailed explanation):

- :config:`GDAL_HTTP_TCP_KEEPALIVE` = YES/NO. whether to enable TCP keep-alive. Defaults to NO
- :config:`GDAL_HTTP_TCP_KEEPIDLE` = integer, in seconds. Keep-alive idle time. Defaults to 60. Only taken into account if GDAL_HTTP_TCP_KEEPALIVE=YES.
- :config:`GDAL_HTTP_TCP_KEEPINTVL` = integer, in seconds. Interval time between keep-alive probes. Defaults to 60. Only taken into account if GDAL_HTTP_TCP_KEEPALIVE=YES.

Starting with GDAL 3.7, the following configuration options control support for SSL client certificates:

- :config:`GDAL_HTTP_SSLCERT` = filename. Filename of the the SSL client certificate. Cf https://curl.se/libcurl/c/CURLOPT_SSLCERT.html
- :config:`GDAL_HTTP_SSLCERTTYPE` = string. Format of the SSL certificate: "PEM" or "DER". Cf https://curl.se/libcurl/c/CURLOPT_SSLCERTTYPE.html
- :config:`GDAL_HTTP_SSLKEY` = filename. Private key file for TLS and SSL client certificate. Cf https://curl.se/libcurl/c/CURLOPT_SSLKEY.html
- :config:`GDAL_HTTP_KEYPASSWD` = string. Passphrase to private key. Cf https://curl.se/libcurl/c/CURLOPT_KEYPASSWD.html

More generally options of :cpp:func:`CPLHTTPFetch` available through configuration options are available.
Starting with GDAL 3.7, the above configuration options can also be specified
as path-specific options with :cpp:func:`VSISetPathSpecificOption`.

The file can be cached in RAM by setting the configuration option :config:`VSI_CACHE` to ``TRUE``. The cache size defaults to 25 MB, but can be modified by setting the configuration option :config:`VSI_CACHE_SIZE` (in bytes). Content in that cache is discarded when the file handle is closed.

Starting with GDAL 2.3, the :config:`CPL_VSIL_CURL_NON_CACHED` configuration option can be set to values like :file:`/vsicurl/http://example.com/foo.tif:/vsicurl/http://example.com/some_directory`, so that at file handle closing, all cached content related to the mentioned file(s) is no longer cached. This can help when dealing with resources that can be modified during execution of GDAL related code. Alternatively, :cpp:func:`VSICurlClearCache` can be used.

Starting with GDAL 2.1, ``/vsicurl/`` will try to query directly redirected URLs to Amazon S3 signed URLs during their validity period, so as to minimize round-trips. This behavior can be disabled by setting the configuration option :config:`CPL_VSIL_CURL_USE_S3_REDIRECT` to ``NO``.

:cpp:func:`VSIStatL` will return the size in st_size member and file nature- file or directory - in st_mode member (the later only reliable with FTP resources for now).

:cpp:func:`VSIReadDir` should be able to parse the HTML directory listing returned by the most popular web servers, such as Apache and Microsoft IIS.

.. _vsicurl_streaming:

/vsicurl_streaming/ (http/https/ftp files: streaming)
+++++++++++++++++++++++++++++++++++++++++++++++++++++

/vsicurl_streaming/ is a file system handler that allows on-the-fly sequential reading of files streamed through HTTP/FTP web protocols, without prior download of the entire file. It requires GDAL to be built against libcurl.

Although this file handler is able seek to random offsets in the file, this will not be efficient. If you need efficient random access and that the server supports range downloading, you should use the :ref:`/vsicurl/ <vsicurl>` file system handler instead.

Recognized filenames are of the form :file:`/vsicurl_streaming/http[s]://path/to/remote/resource` or :file:`/vsicurl_streaming/ftp://path/to/remote/resource`, where :file:`path/to/remote/resource` is the URL of a remote resource.

The :config:`GDAL_HTTP_PROXY` (for both HTTP and HTTPS protocols), :config:`GDAL_HTTPS_PROXY` (for HTTPS protocol only), :config:`GDAL_HTTP_PROXYUSERPWD` and :config:`GDAL_PROXY_AUTH` configuration options can be used to define a proxy server. The syntax to use is the one of Curl ``CURLOPT_PROXY``, ``CURLOPT_PROXYUSERPWD`` and ``CURLOPT_PROXYAUTH`` options.

Starting with GDAL 2.1.3, the :config:`CURL_CA_BUNDLE` or :config:`SSL_CERT_FILE` configuration options can be used to set the path to the Certification Authority (CA) bundle file (if not specified, curl will use a file in a system location).

The file can be cached in RAM by setting the configuration option :config:`VSI_CACHE` to ``TRUE``. The cache size defaults to 25 MB, but can be modified by setting the configuration option :config:`VSI_CACHE_SIZE` (in bytes).

:cpp:func:`VSIStatL` will return the size in st_size member and file nature- file or directory - in st_mode member (the later only reliable with FTP resources for now).

.. _vsis3:

/vsis3/ (AWS S3 files)
++++++++++++++++++++++

/vsis3/ is a file system handler that allows on-the-fly random reading of (primarily non-public) files available in AWS S3 buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

It also allows sequential writing of files. No seeks or read operations are then allowed, so in particular direct writing of GeoTIFF files with the GTiff driver is not supported, unless, if,
starting with GDAL 3.2, the :config:`CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE` configuration option is set to ``YES``, in which case random-write access is possible (involves the creation of a temporary local file, whose location is controlled by the :config:`CPL_TMPDIR` configuration option).
Deletion of files with :cpp:func:`VSIUnlink` is also supported. Starting with GDAL 2.3, creation of directories with :cpp:func:`VSIMkdir` and deletion of (empty) directories with :cpp:func:`VSIRmdir` are also possible.

Recognized filenames are of the form :file:`/vsis3/bucket/key`, where ``bucket`` is the name of the S3 bucket and ``key`` is the S3 object "key", i.e. a filename potentially containing subdirectories.

The generalities of :ref:`/vsicurl/ <vsicurl>` apply.

The following configuration options are specific to the /vsis3/ handler:

-  .. config:: AWS_NO_SIGN_REQUEST
      :choices: YES, NO
      :since: 2.3

      Determines whether to disable request signing.

-  .. config:: AWS_ACCESS_KEY_ID

      Access key ID used for authentication. If using temporary credentials,
      :config:`AWS_SESSION_TOKEN` must be set.

-  .. config:: AWS_SECRET_ACCESS_KEY

      Secret access key associated with :config:`AWS_ACCESS_KEY_ID`.

-  .. config:: AWS_SESSION_TOKEN

      Session token used for validation of temporary credentials
      (:config:`AWS_ACCESS_KEY_ID` and :config:`AWS_SECRET_ACCESS_KEY`)

-  .. config:: CPL_AWS_CREDENTIALS_FILE
      :choices: <filename>

      Location of an AWS credentials file. If not specified, the standard
      location of ``~/.aws/credentials`` will be checked.

-  .. config:: AWS_DEFAULT_PROFILE
      :default: default

      Name of AWS profile.

-  .. config:: AWS_PROFILE
      :default: default
      :since: 3.2

      Name of AWS profile.

-  .. config:: AWS_CONFIG_FILE

      Location of a config file that may provide credentials and the AWS
      region. if not specified the standard location of ``~/.aws/credentials``
      will be checked.

-  .. config:: AWS_ROLE_ARN
      :since: 3.6

      Amazon Resource Name (ARN) specifying the role to use for authentication
      via the `AssumeRoleWithWebIdentity API <https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html>`_.

-  .. config:: AWS_WEB_IDENTITY_TOKEN_FILE
      :choices: <filename>
      :since: 3.6

      Path to file with identity token for use for authentication
      via the `AssumeRoleWithWebIdentity API <https://docs.aws.amazon.com/STS/latest/APIReference/API_AssumeRoleWithWebIdentity.html>`_.

-  .. config:: AWS_REGION
      :default: us-east-1

      Set the AWS region to which requests should be sent. Overridden
      by :config:`AWS_DEFAULT_REGION`.

-  .. config:: AWS_DEFAULT_REGION
      :since: 2.3

      Set the AWS region to which requests should be sent.

-  .. config:: AWS_REQUEST_PAYER
      :choices: requester
      :since: 2.2

      Set to ``requester`` to access a Requester Pays bucket and acknowledge
      associated charges.

-  .. config:: AWS_S3_ENDPOINT
      :default: s3.amazonaws.com

      Allows the use of /vsis3/ with non-AWS remote object stores that use the
      AWS S3 protocol.

-  .. config:: AWS_HTTPS
      :choices: YES, NO
      :default: YES

      If ``YES``, AWS resources will be accessed using HTTPS. If ``NO``, HTTP
      will be used.

-  .. config:: AWS_VIRTUAL_HOSTING
      :choices: TRUE, FALSE
      :default: TRUE

      Select the method of accessing a bucket.
      If ``TRUE``, identifies the bucket via a virtual bucket host name, e.g.: mybucket.cname.domain.com.
      If ``FALSE``, identifies the bucket as the top-level directory in the URI, e.g.: cname.domain.com/mybucket

-  .. config:: VSIS3_CHUNK_SIZE
      :choices: <MB>
      :default: 50

      Set the chunk size for multipart uploads.

-  .. config:: CPL_VSIL_CURL_IGNORE_GLACIER_STORAGE
      :choices: YES, NO
      :default: YES
      :since: 2.4

      When listing a directory, ignore files with ``GLACIER`` storage class.
      Superseded by :config:`CPL_VSIL_CURL_IGNORE_STORAGE_CLASSES`.

-  .. config:: CPL_VSIL_CURL_IGNORE_STORAGE_CLASSES
      :default: GLACIER\,DEEP_ARCHIVE

      Comma-separated list of storage class names that should be ignored when
      listing a directory. If set to empty, objects of all storage classes are
      retrieved).

-  .. config:: CPL_VSIS3_USE_BASE_RMDIR_RECURSIVE
      :choices: YES, NO
      :default: NO
      :since: 3.2

      If ``YES``, recursively delete objects to avoid using batch deletion.

-  .. config:: CPL_VSIS3_CREATE_DIR_OBJECT
      :choices: YES, NO
      :default: YES

      Determines whether to allow :cpp:func:`VSIMkdir` to create an empty object to model an empty directory.

Several authentication methods are possible, and are attempted in the following order:

1. If :config:`AWS_NO_SIGN_REQUEST=YES` configuration option is set, request signing is disabled. This option might be used for buckets with public access rights. Available since GDAL 2.3
2. The :config:`AWS_SECRET_ACCESS_KEY` and :config:`AWS_ACCESS_KEY_ID` configuration options can be set. The :config:`AWS_SESSION_TOKEN` configuration option must be set when temporary credentials are used.
3. Starting with GDAL 2.3, alternate ways of providing credentials similar to what the "aws" command line utility or Boto3 support can be used. If the above mentioned environment variables are not provided, the ``~/.aws/credentials`` or ``%UserProfile%/.aws/credentials`` file will be read (or the file pointed by :config:`CPL_AWS_CREDENTIALS_FILE`). The profile may be specified with the :config:`AWS_DEFAULT_PROFILE` environment variable, or starting with GDAL 3.2 with the :config:`AWS_PROFILE` environment variable (the default profile is "default").
4. The ``~/.aws/config`` or ``%UserProfile%/.aws/config`` file may also be used (or the file pointer by :config:`AWS_CONFIG_FILE`) to retrieve credentials and the AWS region.
5. Starting with GDAL 3.6, if :config:`AWS_ROLE_ARN` and :config:`AWS_WEB_IDENTITY_TOKEN_FILE` are defined we will rely on credentials mechanism for web identity token based AWS STS action AssumeRoleWithWebIdentity (See.: https://docs.aws.amazon.com/eks/latest/userguide/iam-roles-for-service-accounts.html)
6. If none of the above method succeeds, instance profile credentials will be retrieved when GDAL is used on EC2 instances (cf :ref:`vsis3_imds`)

On writing, the file is uploaded using the S3 multipart upload API. The size of chunks is set to 50 MB by default, allowing creating files up to 500 GB (10000 parts of 50 MB each). If larger files are needed, then increase the value of the :config:`VSIS3_CHUNK_SIZE` config option to a larger value (expressed in MB). In case the process is killed and the file not properly closed, the multipart upload will remain open, causing Amazon to charge you for the parts storage. You'll have to abort yourself with other means such "ghost" uploads (e.g. with the s3cmd utility) For files smaller than the chunk size, a simple PUT request is used instead of the multipart upload API.

Since GDAL 3.1, the :cpp:func:`VSIRename` operation is supported (first doing a copy of the original file and then deleting it)

Since GDAL 3.1, the :cpp:func:`VSIRmdirRecursive` operation is supported (using batch deletion method). The :config:`CPL_VSIS3_USE_BASE_RMDIR_RECURSIVE` configuration option can be set to YES if using a S3-like API that doesn't support batch deletion (GDAL >= 3.2). Starting with GDAL 3.6, this can be set as a path-specific option in the :ref:`GDAL configuration file <gdal_configuration_file>`

The :config:`CPL_VSIS3_CREATE_DIR_OBJECT` configuration option can be set to NO to prevent the :cpp:func:`VSIMkdir` operation from creating an empty object with the name of the directory terminated with a slash directory. By default GDAL creates such object, so that empty directories can be modeled, but this may cause compatibility problems with applications that do not expect such empty objects.


Starting with GDAL 3.5, profiles that use IAM role assumption (see https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-role.html) are handled. The ``role_arn`` and ``source_profile`` keywords are required in such profiles. The optional ``external_id``, ``mfa_serial`` and ``role_session_name`` can be specified. ``credential_source`` is not supported currently.

.. _vsis3_imds:

/vsis3/ and AWS Instance Metadata Service (IMDS)
++++++++++++++++++++++++++++++++++++++++++++++++

On EC2 instances, GDAL will try to use the `IMDSv2 <https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/configuring-instance-metadata-service.html>`__ protocol in priority to get the authentication tokens for AWS S3, and fallback to IMDSv1 in case of failure. Note however that on recent Amazon Linux instances, IMDSv1 is no longer accessible, and thus IMDSv2 must be correctly configured (and even if IMDSv1 is available, mis-configured IMDSv2 will cause delays in the authentication step).

There are known issues when running inside a Docker instance in a EC2 instance that require extra configuration of the instance. For example, you need to `increase the hop limit to 2 <https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/instancedata-data-retrieval.html#imds-considerations>`__

There are several ways to do this. One way is to run this command:
::

    aws ec2 modify-instance-metadata-options \
        --instance-id <instance_id> \
        --http-put-response-hop-limit 2 \
        --http-endpoint enabled

Another is to set the HttpPutResponseHopLimit metadata on an AutoScalingGroup LaunchTemplate:
- https://docs.aws.amazon.com/AWSEC2/latest/APIReference/API_InstanceMetadataOptionsRequest.html
- https://docs.aws.amazon.com/AWSCloudFormation/latest/UserGuide/aws-properties-ec2-launchtemplate-metadataoptions.html

Another possibility is to start the Docker container with host networking (``--network=host``), although this breaks isolation of containers by exposing all ports of the host to the container and has thus `security implications <https://stackoverflow.com/a/57051970/40785>`__.

Configuring /vsis3/ with Minio
++++++++++++++++++++++++++++++

The following configuration options can be set to access a
`Minio Docker image <https://min.io/docs/minio/container/index.html>`__

- AWS_VIRTUAL_HOSTING=FALSE
- AWS_HTTPS=NO
- AWS_S3_ENDPOINT="localhost:9000"
- AWS_REGION="us-east-1"
- AWS_SECRET_ACCESS_KEY="your_secret_access_key"
- AWS_ACCESS_KEY_ID="your_access_key"

.. _vsis3_streaming:

/vsis3_streaming/ (AWS S3 files: streaming)
+++++++++++++++++++++++++++++++++++++++++++

/vsis3_streaming/ is a file system handler that allows on-the-fly sequential reading of (primarily non-public) files available in AWS S3 buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

Recognized filenames are of the form :file:`/vsis3_streaming/bucket/key` where ``bucket`` is the name of the S3 bucket and ``key`` is the S3 object "key", i.e. a filename potentially containing subdirectories.

Authentication options, and read-only features, are identical to :ref:`/vsis3/ <vsis3>`

.. versionadded:: 2.1

.. _vsigs:

/vsigs/ (Google Cloud Storage files)
++++++++++++++++++++++++++++++++++++

/vsigs/ is a file system handler that allows on-the-fly random reading of (primarily non-public) files available in Google Cloud Storage buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

Starting with GDAL 2.3, it also allows sequential writing of files. No seeks or read operations are then allowed, so in particular direct writing of GeoTIFF files with the GTiff driver is not supported, unless, if, starting with GDAL 3.2, the :config:`CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE` configuration option is set to ``YES``, in which case random-write access is possible (involves the creation of a temporary local file, whose location is controlled by the :config:`CPL_TMPDIR` configuration option).
Deletion of files with :cpp:func:`VSIUnlink`, creation of directories with :cpp:func:`VSIMkdir` and deletion of (empty) directories with :cpp:func:`VSIRmdir` are also possible.

Recognized filenames are of the form :file:`/vsigs/bucket/key` where ``bucket`` is the name of the bucket and ``key`` is the object "key", i.e. a filename potentially containing subdirectories.

The generalities of :ref:`/vsicurl/ <vsicurl>` apply.

The following configuration options are specific to the /vsigs/ handler:

- .. config:: GS_NO_SIGN_REQUEST
     :choices: YES, NO
     :since: 3.4

     If ``YES``, request signing is disabled.

- .. config:: GS_SECRET_ACCESS_KEY

     Secret for AWS-style authentication (HMAC keys).

- .. config:: GS_ACCESS_KEY_ID

     Access ID for AWS-style authentication (HMAC keys).

- .. config:: GS_OAUTH2_REFRESH_TOKEN

     OAuth2 refresh token. This refresh token can be obtained with the
     :source_file:`swig/python/gdal-utils/osgeo_utils/samples/gdal_auth.py`
     script (``gdal_auth.py -s storage`` or ``gdal_auth.py -s storage-rw``).

- .. config:: GS_OAUTH2_CLIENT_ID

     Client ID to be used when requesting :config:`GS_OAUTH2_REFRESH_TOKEN`.

- .. config:: GS_OAUTH2_CLIENT_SECRET

     Client secret to be used when requesting :config:`GS_OAUTH2_REFRESH_TOKEN`.

- .. :copy-config:`GOOGLE_APPLICATION_CREDENTIALS`

- .. config:: GS_OAUTH2_PRIVATE_KEY

     Private key for OAuth2 authentication. Alternatively, the key may be
     saved in a file and referenced with :config:`GS_OAUTH2_PRIVATE_KEY_FILE`.

- .. config:: GS_OAUTH2_PRIVATE_KEY_FILE
     :choices: <filename>

     Location of private key file for OAuth2 authentication.

- .. config:: GS_OAUTH2_CLIENT_EMAIL

     Client email for OAuth2 authentication, to be used with :config:`GS_OAUTH2_PRIVATE_KEY`
     or :config:`GS_OAUTH2_PRIVATE_KEY_FILE`.

- .. config:: GS_OAUTH2_SCOPE

     Permission scope associated with OAuth2 authentication using :config:`GOOGLE_APPLICATION_CREDENTIALS`.

- .. config:: CPL_GS_CREDENTIALS_FILE
     :default: ~/.boto

     Location of configuration file providing ``gs_secret_access_key`` and
     ``gs_access_key_id``.

- .. config:: GS_USER_PROJECT
     :since: 3.4

     Google Project id (see
     https://cloud.google.com/storage/docs/xml-api/reference-headers#xgooguserproject)
     to charge for requests against Requester Pays buckets.



Several authentication methods are possible, and are attempted in the following order:

1. If :config:`GS_NO_SIGN_REQUEST=YES` configuration option is set, request signing is disabled. This option might be used for buckets with public access rights. Available since GDAL 3.4
2. The :config:`GS_SECRET_ACCESS_KEY` and :config:`GS_ACCESS_KEY_ID` configuration options can be set for AWS-style authentication
3. The :config:`GDAL_HTTP_HEADER_FILE` configuration option to point to a filename of a text file with "key: value" headers. Typically, it must contain a "Authorization: Bearer XXXXXXXXX" line.
4. (GDAL >= 3.7) The :config:`GDAL_HTTP_HEADERS` configuration option can also be set. It must contain at least a line starting with "Authorization:" to be used as an authentication method.
5. (GDAL >= 2.3) The :config:`GS_OAUTH2_REFRESH_TOKEN` configuration option can be set to use OAuth2 client authentication. See http://code.google.com/apis/accounts/docs/OAuth2.html This refresh token can be obtained with the ``gdal_auth.py -s storage`` or ``gdal_auth.py -s storage-rw`` script Note: instead of using the default GDAL application credentials, you may define the :config:`GS_OAUTH2_CLIENT_ID` and :config:`GS_OAUTH2_CLIENT_SECRET` configuration options (need to be defined both for gdal_auth.py and later execution of /vsigs)
6. (GDAL >= 2.3) The :config:`GOOGLE_APPLICATION_CREDENTIALS` configuration option can be set to point to a JSON file containing OAuth2 service account credentials (``type: service_account``), in particular a private key and a client email. See https://developers.google.com/identity/protocols/OAuth2ServiceAccount for more details on this authentication method. The bucket must grant the "Storage Legacy Bucket Owner" or "Storage Legacy Bucket Reader" permissions to the service account. The :config:`GS_OAUTH2_SCOPE` configuration option can be set to change the default permission scope from "https://www.googleapis.com/auth/devstorage.read_write" to "https://www.googleapis.com/auth/devstorage.read_only" if needed.
7. (GDAL >= 3.4.2) The :config:`GOOGLE_APPLICATION_CREDENTIALS` configuration option can be set to point to a JSON file containing OAuth2 user credentials (``type: authorized_user``).
8. (GDAL >= 2.3) Variant of the previous method. The :config:`GS_OAUTH2_PRIVATE_KEY` (or :config:`GS_OAUTH2_PRIVATE_KEY_FILE` and :config:`GS_OAUTH2_CLIENT_EMAIL` can be set to use OAuth2 service account authentication. See https://developers.google.com/identity/protocols/OAuth2ServiceAccount for more details on this authentication method. The :config:`GS_OAUTH2_PRIVATE_KEY` configuration option must contain the private key as a inline string, starting with ``-----BEGIN PRIVATE KEY-----``. Alternatively the :config:`GS_OAUTH2_PRIVATE_KEY_FILE` configuration option can be set to indicate a filename that contains such a private key. The bucket must grant the "Storage Legacy Bucket Owner" or "Storage Legacy Bucket Reader" permissions to the service account. The :config:`GS_OAUTH2_SCOPE` configuration option can be set to change the default permission scope from "https://www.googleapis.com/auth/devstorage.read_write" to "https://www.googleapis.com/auth/devstorage.read_only" if needed.
9. (GDAL >= 2.3) An alternate way of providing credentials similar to what the "gsutil" command line utility or Boto3 support can be used. If the above mentioned environment variables are not provided, the :file:`~/.boto` or :file:`UserProfile%/.boto` file will be read (or the file pointed by :config:`CPL_GS_CREDENTIALS_FILE`) for the gs_secret_access_key and gs_access_key_id entries for AWS style authentication. If not found, it will look for the gs_oauth2_refresh_token (and optionally client_id and client_secret) entry for OAuth2 client authentication.
10. (GDAL >= 2.3) Finally if none of the above method succeeds, the code will check if the current machine is a Google Compute Engine instance, and if so will use the permissions associated to it (using the default service account associated with the VM). To force a machine to be detected as a GCE instance (for example for code running in a container with no access to the boot logs), you can set :config:`CPL_MACHINE_IS_GCE` to ``YES``.

Since GDAL 3.1, the Rename() operation is supported (first doing a copy of the original file and then deleting it).

.. versionadded:: 2.2

.. _vsigs_streaming:

/vsigs_streaming/ (Google Cloud Storage files: streaming)
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/vsigs_streaming/ is a file system handler that allows on-the-fly sequential reading of files (primarily non-public) files available in Google Cloud Storage buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

Recognized filenames are of the form :file:`/vsigs_streaming/bucket/key` where ``bucket`` is the name of the bucket and ``key`` is the object "key", i.e. a filename potentially containing subdirectories.

Authentication options, and read-only features, are identical to :ref:`/vsigs/ <vsigs>`

.. versionadded:: 2.2

.. _vsiaz:

/vsiaz/ (Microsoft Azure Blob files)
++++++++++++++++++++++++++++++++++++

/vsiaz/ is a file system handler that allows on-the-fly random reading of (primarily non-public) files available in Microsoft Azure Blob containers, without prior download of the entire file. It requires GDAL to be built against libcurl.

See :ref:`/vsiadls/ <vsiadls>` for a related filesystem for Azure Data Lake Storage Gen2.

It also allows sequential writing of files. No seeks or read operations are then allowed, so in particular direct writing of GeoTIFF files with the GTiff driver is not supported, unless, if, starting with GDAL 3.2, the :config:`CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE` configuration option is set to ``YES``, in which case random-write access is possible (involves the creation of a temporary local file, whose location is controlled by the :config:`CPL_TMPDIR` configuration option).
A block blob will be created if the file size is below 4 MB. Beyond, an append blob will be created (with a maximum file size of 195 GB).

Deletion of files with :cpp:func:`VSIUnlink`, creation of directories with :cpp:func:`VSIMkdir` and deletion of (empty) directories with :cpp:func:`VSIRmdir` are also possible. Note: when using :cpp:func:`VSIMkdir`, a special hidden :file:`.gdal_marker_for_dir` empty file is created, since Azure Blob does not natively support empty directories. If that file is the last one remaining in a directory, :cpp:func:`VSIRmdir` will automatically remove it. This file will not be seen with :cpp:func:`VSIReadDir`. If removing files from directories not created with :cpp:func:`VSIMkdir`, when the last file is deleted, its directory is automatically removed by Azure, so the sequence ``VSIUnlink("/vsiaz/container/subdir/lastfile")`` followed by ``VSIRmdir("/vsiaz/container/subdir")`` will fail on the :cpp:func:`VSIRmdir` invocation.

Recognized filenames are of the form :file:`/vsiaz/container/key`, where ``container`` is the name of the container and ``key`` is the object "key", i.e. a filename potentially containing subdirectories.

The generalities of :ref:`/vsicurl/ <vsicurl>` apply.

The following configuration options are specific to the /vsiaz/ handler:

- .. config:: AZURE_NO_SIGN_REQUEST
     :choices: YES, NO
     :since: 3.2

     Controls whether requests are signed.

- .. config:: AZURE_STORAGE_CONNECTION_STRING

     Credential string provided in the Access Key section of the administrative interface,
     containing both the account name and a secret key.

- .. config:: AZURE_STORAGE_ACCESS_TOKEN
     :since: 3.5

     Access token typically obtained using Microsoft Authentication Library (MSAL).

- .. config:: AZURE_STORAGE_ACCOUNT

     Specifies storage account name.

- .. config:: AZURE_STORAGE_ACCESS_KEY

     Specifies secret key associated with :config:`AZURE_STORAGE_ACCOUNT`.

- .. config:: AZURE_STORAGE_SAS_TOKEN
     :since: 3.2

     Shared Access Signature.

- .. config:: AZURE_IMDS_OBJECT_ID
     :since: 3.8

     object_id of the managed identity you would like the token for, when using
     Azure Instance Metadata Service (IMDS) authentication in a Azure
     Virtual Matchine. Required if your VM has multiple user-assigned managed identities.
     This option may be set as a path-specific option with :cpp:func:`VSISetPathSpecificOption`

- .. config:: AZURE_IMDS_CLIENT_ID
     :since: 3.8

     client_id of the managed identity you would like the token for, when using
     Azure Instance Metadata Service (IMDS) authentication in a Azure
     Virtual Matchine. Required if your VM has multiple user-assigned managed identities.
     This option may be set as a path-specific option with :cpp:func:`VSISetPathSpecificOption`

- .. config:: AZURE_IMDS_MSI_RES_ID
     :since: 3.8

     msi_res_id (Azure Resource ID) of the managed identity you would like the token for, when using
     Azure Instance Metadata Service (IMDS) authentication in a Azure
     Virtual Matchine. Required if your VM has multiple user-assigned managed identities.
     This option may be set as a path-specific option with :cpp:func:`VSISetPathSpecificOption`

Several authentication methods are possible, and are attempted in the following order:

1. The :config:`AZURE_STORAGE_CONNECTION_STRING` configuration option

2. The :config:`AZURE_STORAGE_ACCOUNT` configuration option is set to specify the account name AND

    a) (GDAL >= 3.5) The :config:`AZURE_STORAGE_ACCESS_TOKEN` configuration option is set to specify the access token, that will be included in a "Authorization: Bearer ${AZURE_STORAGE_ACCESS_TOKEN}" header. This access token is typically obtained using Microsoft Authentication Library (MSAL).
    b) The :config:`AZURE_STORAGE_ACCESS_KEY` configuration option is set to specify the secret key.
    c) The :config:`AZURE_NO_SIGN_REQUEST=YES` configuration option is set, so as to disable any request signing. This option might be used for accounts with public access rights. Available since GDAL 3.2
    d) The :config:`AZURE_STORAGE_SAS_TOKEN` configuration option (``AZURE_SAS`` if GDAL < 3.5) is set to specify a Shared Access Signature. This SAS is appended to URLs built by the /vsiaz/ file system handler. Its value should already be URL-encoded and should not contain any leading '?' or '&' character (e.g. a valid one may look like "st=2019-07-18T03%3A53%3A22Z&se=2035-07-19T03%3A53%3A00Z&sp=rl&sv=2018-03-28&sr=c&sig=2RIXmLbLbiagYnUd49rgx2kOXKyILrJOgafmkODhRAQ%3D"). Available since GDAL 3.2
    e) The current machine is a Azure Virtual Machine with Azure Active Directory permissions assigned to it (see https://docs.microsoft.com/en-us/azure/active-directory/managed-identities-azure-resources/qs-configure-portal-windows-vm). Available since GDAL 3.3.

    Authentication using Azure Active Directory Workload Identity (using AZURE_TENANT_ID, AZURE_CLIENT_ID, AZURE_FEDERATED_TOKEN_FILE and AZURE_AUTHORITY_HOST environment variables), typically for Azure Kubernetes, is available since GDAL 3.7.2

3. Starting with GDAL 3.5, the `configuration file <https://github.com/MicrosoftDocs/azure-docs-cli/blob/main/docs-ref-conceptual/azure-cli-configuration.md>` of the "az" command line utility can be used. The following keys of the ``[storage]`` section will be used in the following priority: ``connection_string``, ``account`` + ``key`` or ``account`` + ``sas_token``

Since GDAL 3.1, the :cpp:func:`VSIRename` operation is supported (first doing a copy of the original file and then deleting it)

Since GDAL 3.3, the :cpp:func:`VSIGetFileMetadata` and :cpp:func:`VSISetFileMetadata` operations are supported.

.. versionadded:: 2.3

.. _vsiaz_streaming:

/vsiaz_streaming/ (Microsoft Azure Blob files: streaming)
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/vsiaz_streaming/ is a file system handler that allows on-the-fly sequential reading of files (primarily non-public) files available in Microsoft Azure Blob containers, buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

Recognized filenames are of the form :file:`/vsiaz_streaming/container/key` where ``container`` is the name of the container and ``key`` is the object "key", i.e. a filename potentially containing subdirectories.

Authentication options, and read-only features, are identical to :ref:`/vsiaz/ <vsiaz>`

.. versionadded:: 2.3

.. _vsiadls:

/vsiadls/ (Microsoft Azure Data Lake Storage Gen2)
++++++++++++++++++++++++++++++++++++++++++++++++++

/vsiadls/ is a file system handler that allows on-the-fly random reading of
(primarily non-public) files available in Microsoft Azure Data Lake Storage file
systems, without prior download of the entire file.
It requires GDAL to be built against libcurl.

It has similar capabilities as :ref:`/vsiaz/ <vsiaz>`, and in particular uses the same
configuration options for authentication. Its advantages over /vsiaz/ are a real
management of directory and Unix-style ACL support. Some features require the Azure
storage to have hierarchical support turned on. Consult its
`documentation <https://docs.microsoft.com/en-us/azure/storage/blobs/data-lake-storage-introduction>`__

The main enhancements over /vsiaz/ are:

  * True directory support (no need for the artificial :file:`.gdal_marker_for_dir`
    empty file that is used for /vsiaz/ to have empty directories)
  * One-call recursive directory deletion with :cpp:func:`VSIRmdirRecursive`
  * Atomic renaming with :cpp:func:`VSIRename`
  * :cpp:func:`VSIGetFileMetadata` support for the "STATUS" and "ACL" metadata domains
  * :cpp:func:`VSISetFileMetadata` support for the "PROPERTIES" and "ACL" metadata domains

.. versionadded:: 3.3

.. _vsioss:

/vsioss/ (Alibaba Cloud OSS files)
++++++++++++++++++++++++++++++++++

/vsioss/ is a file system handler that allows on-the-fly random reading of (primarily non-public) files available in Alibaba Cloud Object Storage Service (OSS) buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

It also allows sequential writing of files. No seeks or read operations are then allowed, so in particular direct writing of GeoTIFF files with the GTiff driver is not supported, unless, if, starting with GDAL 3.2, the :config:`CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE` configuration option is set to ``YES``, in which case random-write access is possible (involves the creation of a temporary local file, whose location is controlled by the :config:`CPL_TMPDIR` configuration option).
Deletion of files with :cpp:func:`VSIUnlink` is also supported. Creation of directories with :cpp:func:`VSIMkdir` and deletion of (empty) directories with :cpp:func:`VSIRmdir` are also possible.

Recognized filenames are of the form :file:`/vsioss/bucket/key` where ``bucket`` is the name of the OSS bucket and ``key`` is the OSS object "key", i.e. a filename potentially containing subdirectories.

The generalities of :ref:`/vsicurl/ <vsicurl>` apply.

The following configuration options are specific to the /vsioss/ handler:

-  .. config:: OSS_SECRET_ACCESS_KEY
      :required: YES

      Secret access key for authentication.

-  .. config:: OSS_ACCESS_KEY_ID
      :required: YES

      Access key ID for authentication.

-  .. config:: OSS_ENDPOINT
      :default: oss-us-east-1.aliyuncs.com

      Endpoint URL containing the region associated with the bucket.

-  .. config:: VSIOSS_CHUNK_SIZE
      :choices: <MB>
      :default: 50

      Chunk size used with multipart upload API.

The :config:`OSS_SECRET_ACCESS_KEY` and :config:`OSS_ACCESS_KEY_ID` configuration options must be set. The :config:`OSS_ENDPOINT` configuration option should normally be set to the appropriate value, which reflects the region attached to the bucket. If the bucket is stored in another region than oss-us-east-1, the code logic will redirect to the appropriate endpoint.

On writing, the file is uploaded using the OSS multipart upload API. The size of chunks is set to 50 MB by default, allowing creating files up to 500 GB (10000 parts of 50 MB each). If larger files are needed, then increase the value of the :config:`VSIOSS_CHUNK_SIZE` config option to a larger value (expressed in MB). In case the process is killed and the file not properly closed, the multipart upload will remain open, causing Alibaba to charge you for the parts storage. You'll have to abort yourself with other means. For files smaller than the chunk size, a simple PUT request is used instead of the multipart upload API.

.. versionadded:: 2.3

.. _vsioss_streaming:

/vsioss_streaming/ (Alibaba Cloud OSS files: streaming)
+++++++++++++++++++++++++++++++++++++++++++++++++++++++

/vsioss_streaming/ is a file system handler that allows on-the-fly sequential reading of files (primarily non-public) files available in Alibaba Cloud Object Storage Service (OSS) buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

Recognized filenames are of the form :file:`/vsioss_streaming/bucket/key` where ``bucket`` is the name of the bucket and ``key`` is the object "key", i.e. a filename potentially containing subdirectories.

Authentication options, and read-only features, are identical to :ref:`/vsioss/ <vsioss>`

.. versionadded:: 2.3

.. _vsiswift:

/vsiswift/ (OpenStack Swift Object Storage)
+++++++++++++++++++++++++++++++++++++++++++

/vsiswift/ is a file system handler that allows on-the-fly random reading of (primarily non-public) files available in OpenStack Swift Object Storage (swift) buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

It also allows sequential writing of files. No seeks or read operations are then allowed, so in particular direct writing of GeoTIFF files with the GTiff driver is not supported, unless, if, starting with GDAL 3.2, the :config:`CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE` configuration option is set to ``YES``, in which case random-write access is possible (involves the creation of a temporary local file, whose location is controlled by the :config:`CPL_TMPDIR` configuration option).
Deletion of files with :cpp:func:`VSIUnlink` is also supported. Creation of directories with :cpp:func:`VSIMkdir` and deletion of (empty) directories with :cpp:func:`VSIRmdir` are also possible.

Recognized filenames are of the form :file:`/vsiswift/bucket/key` where ``bucket`` is the name of the swift bucket and ``key`` is the swift object "key", i.e. a filename potentially containing subdirectories.

The generalities of :ref:`/vsicurl/ <vsicurl>` apply.

The following configuration options are specific to the /vsioss/ handler:

- .. config:: SWIFT_STORAGE_URL

     Storage URL.

- .. config:: SWIFT_AUTH_TOKEN

     Value of the x-auth-token authorization

- .. config:: SWIFT_AUTH_V1_URL

     URL for Auth V1 authentication.

- .. config:: SWIFT_USER

     User name for Auth V1 authentication.

- .. config:: SWIFT_KEY

     Key for Auth V1 authentication.

Three authentication methods are possible, and are attempted in the following order:

1. The :config:`SWIFT_STORAGE_URL` and :config:`SWIFT_AUTH_TOKEN` configuration options are set respectively to the storage URL (e.g http://127.0.0.1:12345/v1/AUTH_something) and the value of the x-auth-token authorization token.
2. The :config:`SWIFT_AUTH_V1_URL`, :config:`SWIFT_USER` and :config:`SWIFT_KEY` configuration options are set respectively to the endpoint of the Auth V1 authentication (e.g http://127.0.0.1:12345/auth/v1.0), the user name and the key/password. This authentication endpoint will be used to retrieve the storage URL and authorization token mentioned in the first authentication method.
3. Authentication with Keystone v3 is using the same options as python-swiftclient, see https://docs.openstack.org/python-swiftclient/latest/cli/index.html#authentication for more details. GDAL (>= 3.1) supports the following options:

   - `OS_IDENTITY_API_VERSION=3`
   - `OS_AUTH_URL`
   - `OS_USERNAME`
   - `OS_PASSWORD`
   - `OS_USER_DOMAIN_NAME`
   - `OS_PROJECT_NAME`
   - `OS_PROJECT_DOMAIN_NAME`
   - `OS_REGION_NAME`

4. Application Credential Authentication via Keystone v3, GDAL (>= 3.3.1) supports application-credential authentication with the following options:

   - `OS_IDENTITY_API_VERSION=3`
   - `OS_AUTH_TYPE=v3applicationcredential`
   - `OS_AUTH_URL`
   - `OS_APPLICATION_CREDENTIAL_ID`
   - `OS_APPLICATION_CREDENTIAL_SECRET`
   - `OS_REGION_NAME`

This file system handler also allows sequential writing of files (no seeks or read operations are then allowed).

In some versions of OpenStack Swift, the access to large (segmented) files fails unless they are explicitly marked as static large objects, instead of being dynamic large objects which is the default. Using the python-swiftclient this can be achieved when uploading the file by passing the ``--use-slo`` flag (see https://docs.openstack.org/python-swiftclient/latest/cli/index.html#swift-upload for all options). For more information about large objects see https://docs.openstack.org/swift/latest/api/large_objects.html.

.. versionadded:: 2.3

.. _vsiswift_streaming:

/vsiswift_streaming/ (OpenStack Swift Object Storage: streaming)
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/vsiswift_streaming/ is a file system handler that allows on-the-fly sequential reading of files (primarily non-public) files available in OpenStack Swift Object Storage (swift) buckets, without prior download of the entire file. It requires GDAL to be built against libcurl.

Recognized filenames are of the form :file:`/vsiswift_streaming/bucket/key` where ``bucket`` is the name of the bucket and ``key`` is the object "key", i.e. a filename potentially containing subdirectories.

Authentication options, and read-only features, are identical to :ref:`/vsiswift/ <vsiswift>`

.. versionadded:: 2.3

.. _vsihdfs:

/vsihdfs/ (Hadoop File System)
++++++++++++++++++++++++++++++

/vsihdfs/ is a file system handler that provides read access to HDFS.
This handler requires GDAL to have been built with Java support
(CMake `FindJNI <https://cmake.org/cmake/help/latest/module/FindJNI.html>`__)
and :ref:`HDFS <building_from_source_hdfs>` support.
Support for this handler is currently only available on Unix-like systems.

Note: support for the HTTP REST API (webHdfs) is also available with :ref:`vsiwebhdfs`

The LD_LIBRARY_PATH and CLASSPATH environment variables must be typically
set up as following.

::

    HADOOP_HOME=$HOME/hadoop-3.3.5
    LD_LIBRARY_PATH=$HADOOP_HOME/lib/native:$LD_LIBRARY_PATH
    CLASSPATH=$HADOOP_HOME/etc/hadoop:$HADOOP_HOME/share/hadoop/common/*:$HADOOP_HOME/share/hadoop/common/lib/*:$HADOOP_HOME/share/hadoop/hdfs/*


Failure to properly define the CLASSPATH will result in hard crashes in the
native libhdfs.

Relevant Hadoop documentation links:

- `C API libhdfs <https://hadoop.apache.org/docs/stable/hadoop-project-dist/hadoop-hdfs/LibHdfs.html>`__
- `HDFS Users Guide <https://hadoop.apache.org/docs/stable/hadoop-project-dist/hadoop-hdfs/HdfsUserGuide.html>`__
- `Hadoop: Setting up a Single Node Cluster <https://hadoop.apache.org/docs/stable/hadoop-project-dist/hadoop-common/SingleCluster.html>`__

Recognized filenames are of the form :file:`/vsihdfs/hdfsUri` where ``hdfsUri`` is a valid HDFS URI.

Examples:

::

    /vsihdfs/file:/home/user//my.tif  (a local file accessed through HDFS)
    /vsihdfs/hdfs://localhost:9000/my.tif  (a file stored in HDFS)

.. versionadded:: 2.4

.. _vsiwebhdfs:

/vsiwebhdfs/ (Web Hadoop File System REST API)
++++++++++++++++++++++++++++++++++++++++++++++

/vsiwebhdfs/ is a file system handler that provides read and write access to HDFS through its HTTP REST API.

Recognized filenames are of the form :file:`/vsiwebhdfs/http://hostname:port/webhdfs/v1/path/to/filename`.

Examples:

::

    /vsiwebhdfs/http://localhost:50070/webhdfs/v1/mydir/byte.tif

It also allows sequential writing of files. No seeks or read operations are then allowed, so in particular direct writing of GeoTIFF files with the GTiff driver is not supported, unless, if, starting with GDAL 3.2, the :config:`CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE` configuration option is set to ``YES``, in which case random-write access is possible (involves the creation of a temporary local file, whose location is controlled by the :config:`CPL_TMPDIR` configuration option).
Deletion of files with :cpp:func:`VSIUnlink` is also supported. Creation of directories with :cpp:func:`VSIMkdir` and deletion of (empty) directories with :cpp:func:`VSIRmdir` are also possible.

The generalities of :ref:`/vsicurl/ <vsicurl>` apply.

The following configuration options are available:

- .. config:: WEBHDFS_USERNAME

     User name (when security is off).

- .. config:: WEBHDFS_DELEGATION

     Hadoop delegation token (when security is on).

- .. config:: WEBHDFS_DATANODE_HOST

     For APIs using redirect, substitute the redirection hostname with the one provided by this option (normally resolvable hostname should be rewritten by a proxy)

- .. config:: WEBHDFS_REPLICATION
     :choices: <integer>

     Replication value used when creating a file

- .. config:: WEBHDFS_PERMISSION
     :choices: <integer>

     Permission mask (to provide as decimal number) when creating a file or directory

This file system handler also allows sequential writing of files (no seeks or read operations are then allowed)

.. versionadded:: 2.4

.. _vsistdin:

/vsistdin/ (standard input streaming)
-------------------------------------

/vsistdin/ is a file handler that allows reading from the standard input stream.

The filename syntax must be only :file:`/vsistdin/`, (not e.g.,
/vsistdin/path/to/f.csv , but "/vsistdin?buffer_limit=value" is OK.)

The file operations available are of course limited to Read() and forward Seek().
Full seek in the first MB of a file is possible, and it is cached so that closing,
re-opening :file:`/vsistdin/` and reading within this first megabyte is possible
multiple times in the same process.

The size of the in-memory cache can be controlled with
the :config:`CPL_VSISTDIN_BUFFER_LIMIT` configuration option:

- .. config:: CPL_VSISTDIN_BUFFER_LIMIT
     :default: 1MB
     :since: 3.6

     Specifies the size of the /vsistdin in bytes
     (or using a MB or GB suffix, e.g. "1GB"), or -1 for unlimited.

The "/vsistdin?buffer_limit=value" syntax can also be used.

/vsistdin filenames can be combined with other file system. For example, to
read a file within a potentially big ZIP file streamed to gdal_translate:

::

    cat file.tif.zip | gdal_translate /vsizip/{/vsistdin?buffer_limit=-1}/path/to/some.tif out.tif


.. _vsistdout:

/vsistdout/ (standard output streaming)
---------------------------------------

/vsistdout/ is a file handler that allows writing into the standard output stream.

The filename syntax must be only :file:`/vsistdout/`.

The file operations available are of course limited to Write().

A variation of this file system exists as the :file:`/vsistdout_redirect/` file system handler, where the output function can be defined with :cpp:func:`VSIStdoutSetRedirection`.

.. _vsimem:

/vsimem/ (in-memory files)
--------------------------

/vsimem/ is a file handler that allows block of memory to be treated as files. All portions of the file system underneath the base path :file:`/vsimem/` will be handled by this driver.

Normal VSI*L functions can be used freely to create and destroy memory arrays, treating them as if they were real file system objects. Some additional methods exist to efficiently create memory file system objects without duplicating original copies of the data or to "steal" the block of memory associated with a memory file. See :cpp:func:`VSIFileFromMemBuffer` and :cpp:func:`VSIGetMemFileBuffer`.

Directory related functions are supported.

/vsimem/ files are visible within the same process. Multiple threads can access the same underlying file in read mode, provided they used different handles, but concurrent write and read operations on the same underlying file are not supported (locking is left to the responsibility of calling code).

.. _vsisubfile:

/vsisubfile/ (portions of files)
--------------------------------

The /vsisubfile/ virtual file system handler allows access to subregions of files, treating them as a file on their own to the virtual file system functions (VSIFOpenL(), etc).

A special form of the filename is used to indicate a subportion of another file: :file:`/vsisubfile/<offset>[_<size>],<filename>`.

The size parameter is optional. Without it the remainder of the file from the start offset as treated as part of the subfile. Otherwise only <size> bytes from <offset> are treated as part of the subfile. The <filename> portion may be a relative or absolute path using normal rules. The <offset> and <size> values are in bytes.

Examples:

::

    /vsisubfile/1000_3000,/data/abc.ntf
    /vsisubfile/5000,../xyz/raw.dat

Unlike the /vsimem/ or conventional file system handlers, there is no meaningful support for filesystem operations for creating new files, traversing directories, and deleting files within the /vsisubfile/ area. Only the :cpp:func:`VSIStatL`, :cpp:func:`VSIFOpenL` and operations based on the file handle returned by :cpp:func:`VSIFOpenL` operate properly.

.. _vsisparse:

/vsisparse/ (sparse files)
--------------------------

The /vsisparse/ virtual file handler allows a virtual file to be composed from chunks of data in other files, potentially with large spaces in the virtual file set to a constant value. This can make it possible to test some sorts of operations on what seems to be a large file with image data set to a constant value. It is also helpful when wanting to add test files to the test suite that are too large, but for which most of the data can be ignored. It could, in theory, also be used to treat several files on different file systems as one large virtual file.

The file referenced by /vsisparse/ should be an XML control file formatted something like:

::

    <VSISparseFile>
        <Length>87629264</Length>
        <SubfileRegion>  <!-- Stuff at start of file. -->
            <Filename relative="1">251_head.dat</Filename>
            <DestinationOffset>0</DestinationOffset>
            <SourceOffset>0</SourceOffset>
            <RegionLength>2768</RegionLength>
        </SubfileRegion>

        <SubfileRegion>  <!-- RasterDMS node. -->
            <Filename relative="1">251_rasterdms.dat</Filename>
            <DestinationOffset>87313104</DestinationOffset>
            <SourceOffset>0</SourceOffset>
            <RegionLength>160</RegionLength>
        </SubfileRegion>

        <SubfileRegion>  <!-- Stuff at end of file. -->
            <Filename relative="1">251_tail.dat</Filename>
            <DestinationOffset>87611924</DestinationOffset>
            <SourceOffset>0</SourceOffset>
            <RegionLength>17340</RegionLength>
        </SubfileRegion>

        <ConstantRegion>  <!-- Default for the rest of the file. -->
            <DestinationOffset>0</DestinationOffset>
            <RegionLength>87629264</RegionLength>
            <Value>0</Value>
        </ConstantRegion>
    </VSISparseFile>

Hopefully the values and semantics are fairly obvious.


.. _vsicached:

/vsicached/ (File caching)
--------------------------

The :cpp:func:`VSICreateCachedFile` function takes a virtual file handle and returns a new handle that caches read-operations on the input file handle. The cache is RAM based and the content of the cache is discarded when the file handle is closed. The cache is a least-recently used lists of blocks of 32KB each (default size).

This is mostly useful for files accessible through slow local/operating-system-mounted filesystems.

That is implicitly used by a number of the above mentioned file systems (namely the default one for standard file system operations, and the /vsicurl/ and other related network file systems) if the ``VSI_CACHE`` configuration option is set to ``YES``.

The default size of caching for each file is 25 MB (25 MB for each file that is cached), and can be controlled with the ``VSI_CACHE_SIZE`` configuration option (value in bytes).

The :cpp:class:`VSICachedFile` class only handles read operations at that time, and will error out on write operations.

Starting with GDAL 3.8, a ``/vsicached?`` virtual file system also exists to cache a particular file.

The syntax is the following one: ``/vsicached?[option_i=val_i&]*file=<filename>``
where each option name and value (including the value of ``file``) is URL-encoded
(actually, only required for the ampersand character. It might be desirable to
have forward slash character uncoded).
It is important that the ``file`` option appears at the end, so that code that
tries to look for side-car files, list directory content, can work properly.

Currently supported options are:

- ``chunk_size=<value>`` where value is the` size of the chunk size in bytes. ``KB`` or ``MB`` suffixes can be also appended (without space after the numeric value). The maximum supported value is 1 GB.
- ``cache_size=<value>`` where value is the size of the cache size in bytes, for each file. ``KB`` or ``MB`` suffixes can be also appended.

Examples:

- ``/vsicached?chunk_size=1MB&file=/home/even/byte.tif``
- ``/vsicached?file=./byte.tif``


.. _vsicrypt:

/vsicrypt/ (encrypted files)
----------------------------

/vsicrypt/ is a special file handler is installed that allows reading/creating/update encrypted files on the fly, with random access capabilities.

Refer to :cpp:func:`VSIInstallCryptFileHandler` for more details.
