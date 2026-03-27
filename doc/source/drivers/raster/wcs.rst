.. _raster.wcs:

================================================================================
WCS -- OGC Web Coverage Service
================================================================================

.. shortname:: WCS

.. build_dependencies:: libcurl

The optional GDAL WCS driver allows access to coverages served by an OGC
Web Coverage Service (WCS) as raster datasets. GDAL acts as a client to
the WCS server.

Accessing a WCS server involves a local service description file containing a
single ``<WCS_GDAL>`` XML element. No additional content (including
whitespace outside the root element) is allowed.
Starting with GDAL 2.3, this file is typically managed automatically by
the driver in a cache directory. Users are encouraged to configure its
contents through driver options rather than editing it manually.

The dataset name uses the syntax :samp:`WCS:<URL>`, where ``<URL>`` is the
WCS service endpoint. Optional parameters may be appended to specify the
WCS version, coverage identifier, and other request options.

If the URL does not include a coverage name, the driver attempts to
fetch the capabilities document from the server, parse it, and show the
resulting metadata to the user. Available coverages are listed as subdatasets.
If the URL includes a coverage name (the parameter key 'coverage' can be
used irrespective of the WCS version), the driver retrieves and parses the
coverage description document and creates (or updates) the service
description file accordingly. A small test GetCoverage request may be
issued to determine details of the served data.
If the server capabilities document is not already cached, it will also
be retrieved.

When using WCS version 2.0.1 (for which support is available starting at
GDAL version 2.3), additional parameters can be provided to
control how the coverage data model is mapped to the GDAL raster data
model. If a coverage has more than two dimensions, the driver will append the coverage
metadata but report zero bands. In this case, the user must provide driver options to specify how to
handle the additional dimensions and data fields. See :ref:`raster.wcs.subsetting`.

The WCS driver provides basic support for WCS versions 1.0.0, 1.1.0, 1.1.1, 1.1.2, and 2.0.1.
Any single-file coverage in a format supported by GDAL should work.
By default, the driver prefers a format containing "tiff" in its name; if none is available,
it falls back to the first format offered by the server or a user can specify a
preferred format using driver options. Coordinate systems are read from the DescribeCoverage response.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Service description file
------------------------

The service description file has the following elements as immediate
children of the document element. Note that when the :samp:`WCS:<URL>` syntax
for a dataset name is used, the contents of the service description file
is meant to be modified by using options.

-  **ServiceURL**: URL of the service without parameters
-  **Version**: The WCS version that is used in the communication. If
   the dataset name syntax :samp:`WCS:<URL>` is used the default is 2.0.1 and the
   server's response may change the user request, otherwise defaults to
   1.0.0. Versions 1.0.0, 1.1.0, 1.1.1, 1.1.2, and 2.0.1 are supported.
-  **CoverageName**: The coverage that is used for the dataset.
-  **Format**: The format to use for GetCoverage calls. If not set,
   selected by the driver. (WCS version 2.0)
-  **PreferredFormat**: The format to use for GetCoverage calls. If not
   set, selected by the driver. (WCS versions 1.0 and 1.1)
-  **Interpolation**: The interpolation method used when scaling. Should
   be one of the server supported values.
-  **BlockXSize**: The block width to use for block cached remote
   access.
-  **BlockYSize**: The block height to use for block cached remote
   access.
-  **OverviewCount**: The number of overviews to represent bands as
   having. Defaults to a number such that the top overview is fairly
   smaller (less than 1K x 1K or so).
-  **NoDataValue**: The nodata value to use for all the bands (blank for
   none). Normally determined by the driver. With version 2.0.1 this may
   be a comma separated list of values, one for each band.

   **Elements for controlling the range and domain:**

-  **Domain**: The axes that are used for the spatial dimensions. The
   default is to use the first two axes given by the server. The axis
   order swap may apply. Syntax: ``axis_name,axis_name`` A
   ``field_name:field_name`` in the list denotes a range of fields. (Used
   only with version 2.0.1)
-  **DefaultTime**: A timePosition to use by default when accessing
   coverages with a time dimension. Populated with the last offered time
   position by default. (Used only with version 1.0.0)
-  **FieldName**: Name of the field being accessed. Used only with
   version 1.1. Defaults to the first field in the DescribeCoverage. In
   version 1.1 the range consists of one or more fields, which may be
   scalar or vector. A vector field contains one or more bands.
-  **BandCount**: Number of bands in the dataset, normally determined by
   the driver.
-  **BandType**: The pixel data type to use. Normally determined by the
   driver.

   **Elements for controlling the communication:**

-  **Timeout**: The timeout to use for remote service requests. If not
   provided, the libcurl default is used.
-  **UserPwd**: May be supplied with ``userid:password`` to pass a userid
   and password to the remote server.
-  **HttpAuth**: May be BASIC, NTLM or ANY to control the authentication
   scheme to be used.
-  **GetCoverageExtra**: An additional set of keywords to add to
   GetCoverage requests in URL encoded form. e.g.
   ``&RESAMPLE=BILINEAR&Band=1``. Note that the extra parameters should
   not be known parameters (see below).
-  **DescribeCoverageExtra**: An additional set of keywords to add to
   DescribeCoverage requests in URL encoded form. e.g. ``&CustNo=775``

   **Elements that may be needed to deal with server quirks (GDAL 2.3):**

   **Note:** The options are not propagated to the subdataset with the
   switch ``-sd``.

-  **OriginAtBoundary**: Set this flag if the server reports grid origin
   to be at the pixel boundary instead of the pixel center. (Use for
   MapServer versions <= 7.0.7 with WCS versions 1.0.0 and 2.0.1)
-  **OuterExtents**: Set to consider WCS 1.1 extents as boundaries of
   outer pixels instead of centers of outer pixels. (Use for GeoServer).
-  **BufSizeAdjust**: Set to 0.5 in WCS 1.1 if data access fails due to
   the response not having expected size. (Use for GeoServer).
-  **OffsetsPositive**: Use with MapServer in WCS version 2.0.1 together
   with NrOffsets.
-  **NrOffsets**: Set to 2 if the server requires that there are only
   two values in the GridOffsets. Use when the server is MapServer or
   ArcGIS. With MapServer use also OffsetsPositive.
-  **GridCRSOptional**: Let the driver skip Grid\* parameters from a WCS
   1.1 GetCoverage request if the request is not scaled. Do not use for
   GeoServer.
-  **NoGridAxisSwap**: Set to tell the driver not to swap axis order.
   When reading the grid geometry (in WCS 1.1 the origin and offsets, in
   WCS 2.0.1 the grid envelope, axis labels, and offsets) no axis order
   swap is done although it would otherwise be done if this flag is set.
   In 1.1 it would be done if the CRS has inverted axes. In 2.0.1 it
   would be done if the axisOrder of the sequenceRule in GridFunction
   defines so. This is needed usually both in 1.1 and 2.0.1 when parsing
   coverage descriptions from MapServer and GeoServer.
-  **SubsetAxisSwap** Set to tell the driver to swap the axis names in
   boundedBy.Envelope.axisLabels when making WCS 2.0.1 GetCoverage
   request. Needed for GeoServer when EPSG dictates axis order swap.
-  **UseScaleFactor**: Set to tell the driver to use scale by factor
   approach instead of scale to size when making a WCS 2.0.1 GetCoverage
   request. Required when the server is ArcGIS.

.. _raster.wcs.subsetting:

Range and dimension subsetting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When using WCS version 2.0.1, the range (fields/bands) and the
dimension can and/or may need to be subsetted. If the data model of the
coverage contains dimensions beyond the two geographic or map
coordinates, those dimensions must be sliced for GDAL. The coverage may
also contain a large number of fields, from which only a subset
may be selected for inclusion in the GDAL dataset.

Range and dimension subsetting must be specified via URL parameters,
since a single coverage can produce multiple distinct GDAL datasets.
In the WCS cache, this results in separate sets of files corresponding to each GDAL dataset:

#. server Capabilities file and a GDAL dataset metadata file made from
   it (key = URL with WCS version number)
#. server DescribeCoverage file, a template GDAL service file made from
   it, and a GDAL dataset metadata file made for it (key = URL with WCS
   version number and coverage name)
#. the GDAL service file specifically for this dataset, and a GDAL
   dataset metadata file (key = URL with WCS version number, coverage
   name, and range and dimension subsetting parameters)

The following URL parameters are used to control the range and dimension
subsetting. Note that these can also be set through options into the
service file. The ones in URL take precedence.

-  **RangeSubset**: Used to select a subset of coverage fields to the
   dataset. Syntax: ``field_name,field_name:field_name,..`` (Note:
   requires that the server implements the range subsetting extension.)
-  **Subset**: Trim or slice a dimension when fetching data from the
   server. Syntax:
   *axis_name(trim_begin_value,trim_end_value);axis_name(slice_value)*
   Note that trimming the geographic/map coordinates is done by the
   driver.

Other WCS parameters
~~~~~~~~~~~~~~~~~~~~

The following parameters (for WCS version 2.0.1) are supported in addition to those described above.
They can be specified either via driver options or directly in the URL, with URL parameters taking precedence.
Note that support for these parameters depends on the server; not all servers may recognize them.

-  MediaType
-  UpdateSequence
-  GEOTIFF:COMPRESSION
-  GEOTIFF:JPEG_QUALITY
-  GEOTIFF:PREDICTOR
-  GEOTIFF:INTERLEAVE
-  GEOTIFF:TILING
-  GEOTIFF:TILEWIDTH

Open options
~~~~~~~~~~~~

When the :samp:`WCS:<URL>` dataset name syntax is used, open options are used
to control the driver and the contents of the service description file.
In the case the URL does not contain coverage name, the service
description file is not used and thus in that case the options are not
written into it. Open options are given separate to the dataset name,
with GDAL utility programs they are given using the -oo switch
(`-oo "NAME=VALUE"`). The -oo switch expects only one option but more
options can be given repeating the switch.

In addition to DescribeCoverageExtra and GetCoverageExtra, which are
stored in the service description file, there is also
GetCapabilitiesExtra, which can be used as an open option when
requesting the overall capabilities from the server. The open option
SKIP_GETCOVERAGE can be used to prevent the driver making a GetCoverage
request to the server, which it usually does if it can't determine the
band count and band data type from the capabilities or coverage
descriptions. This option may be needed if GetCoverage request fails.

All above listed element names can be given as options to the WCS
driver. In the case of flags the option should formally be "Name=TRUE",
but only "Name" suffices.

The cache
~~~~~~~~~

When using the :samp:`WCS:<URL>` dataset name syntax, server responses,
the service description file, and metadata files are stored in a cache.
If the required resources are already cached, they will be used directly,
avoiding additional requests to the server.

The default location of the cache directory is ``$HOME/.gdal/wcs_cache`` on Linux,
and ``%USERPROFILE%\.gdal\wcs_cache`` on Windows.

The cache contents can be seen as subdatasets using an empty URL:

   .. code-block:: bash

    gdalinfo "WCS:"

    # or using the gdal CLI
    gdal raster info "WCS:"

The cache control options/flags are

-  **CACHE=path** Overrides the default cache location.
-  **CLEAR_CACHE=YES/NO** The cache is completely initialized and all files are
   deleted.
-  **REFRESH_CACHE=YES/NO** The cache entry, either capabilities or coverage,
   depending on the call at hand, is deleted.
-  **DELETE_FROM_CACHE=k** The cache entry (subdataset k), is deleted.

The WCS: dataset name syntax
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The URL in the dataset name is not a complete WCS request URL. The
request URL, specifically, its query part, for GetCapabilities,
DescribeCoverage, and GetCoverage requests is composed by the driver.
Typically the user should only need to add to the server address the
version and coverage parameters. The string 'coverage' can be used as
the coverage parameter key although different WCS versions use different
keys. 'coverage' is also always used in the cache key.

The user may add arbitrary standard and non-standard extra parameters to
the URL. However, when that is done, it should be noted that the URL is
a cache database key and capability documents are linked to coverage
documents through the key. Please consider using the Extra open options.

Time
~~~~

This driver includes experimental support for
time based WCS 1.0.0 servers. On initial access the last offered time
position will be identified as the DefaultTime. Each time position
available for the coverage will be treated as a subdataset.

Note that time based subdatasets are not supported when the service
description is the filename. Currently time support is not available for
versions other than WCS 1.0.0.

Examples
~~~~~~~~

.. example::
   :title: A gdalinfo call to a coverage served by MapServer

   .. code-block:: bash

       gdalinfo \
       -oo INTERLEAVE=PIXEL \
       -oo OffsetsPositive \
       -oo NrOffsets=2 \
       -oo NoGridAxisSwap \
       -oo BandIdentifier=none \
       "WCS:http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows?VERSION=2.0.1&coverage=BGS_EMODNET_CentralMed-MCol"

.. example::
   :title: A gdal_translate call to a scaled clip of a coverage served by GeoServer

   .. code-block:: bash

       gdal_translate \
       -oo CACHE=wcs_cache \
       -oo CLEAR_CACHE \
       -oo INTERLEAVE=PIXEL \
       -projwin 377418 6683393.87938218 377717.879386966 6683094 \
       -oo Subset="time(1985-01-01T00:00:00.000Z)" \
       -outsize 60 0 \
       "WCS:https://beta-karttakuva.maanmittauslaitos.fi/wcs/service/ows?version=2.0.1&coverage=ortokuva__ortokuva" \
       scaled.tiff

.. example::
   :title: List coverages available at a WCS endpoint, refreshing any cached responses

   .. code-block:: bash

       gdal raster info "WCS:https://geo.weather.gc.ca/geomet" --oo REFRESH_CACHE=YES

.. example::
   :title: Save a WCS request as a GeoTIFF

   .. code-block:: bash

       gdal raster convert  "WCS:https://geo.weather.gc.ca/geomet?version=2.0.1&coverage=WCPS_1km_PrecipRate" \
                --oo "Subset=lat(46,48.5)" \
                --oo "Subset=lon(-90,-86)" \
                out.tif

.. example::
   :title: Use a pipeline to download and render data from a WCS endpoint

   .. tabs::

      .. code-tab:: bash

        cat > colors.txt <<EOF
        0 0 0 0
        5e-08 0 0 255
        1e-07 0 255 0
        1e-06 255 255 0
        3.8e-06 255 0 0
        EOF

        gdal raster pipeline \
            ! read "WCS:https://geo.weather.gc.ca/geomet?version=2.0.1&coverage=WCPS_1km_PrecipRate" \
                --oo "Subset=lat(46,48.5)" \
                --oo "Subset=lon(-90,-86)" \
            ! color-map --color-map=colors.txt \
            ! write output.jpg --overwrite

      .. code-tab:: ps1

        $colormap = @"
        0 0 0 0
        5e-08 0 0 255
        1e-07 0 255 0
        1e-06 255 255 0
        3.8e-06 255 0 0
        "@

        $colormap | Set-Content -Path "colors.txt"

        gdal raster pipeline `
            ! read "WCS:https://geo.weather.gc.ca/geomet?version=2.0.1&coverage=WCPS_1km_PrecipRate" `
                --oo "Subset=lat(46,48.5)" `
                --oo "Subset=lon(-90,-86)" `
            ! color-map --color-map=colors.txt `
            ! write output.jpg --overwrite

See Also
--------

-  `OGC WCS Standards <http://www.opengeospatial.org/standards/wcs>`__
