.. _raster.wcs:

================================================================================
WCS -- OGC Web Coverage Service
================================================================================

.. shortname:: WCS

.. build_dependencies:: libcurl

The optional GDAL WCS driver allows use of a coverage in a WCS server as
a raster dataset. GDAL acts as a client to the WCS server.

Accessing a WCS server is accomplished by creating a local service
description file that contains one `<WCS_GDAL>` XML element. It is
important that there is no spaces or other content outside that element.
Starting at version 2.3 the service description file is meant to be
managed by the driver in a cache directory. User should control the
contents of the service file using options. The dataset name is
:samp:`WCS:<URL>`, where the <URL> is the URL of the server appended
potentially appended with WCS version, coverage, and possibly other
parameters. If the WCS version is 2.0.1 further parameters can be given
to control how the data model of the coverage is mapped to the GDAL data
model.

If the URL does not contain a coverage name, the driver attempts to
fetch the capabilities document from the server, parse it, and show the
resulting metadata to the user. Coverages are shown as subdatasets. If
the URL contains a coverage name as parameter (the key 'coverage' can be
used irrespective of the WCS version), the driver attempts to fetch the
coverage description document from the server, parse it, and create
service description file. A small test GetCoverage request may be done
to obtain details of the served data. If the respective server
capabilities file is not cached, it will also be fetched.

With service version 2.0.1 (for which support is available starting at
GDAL version 2.3), it may be that the coverage has more than two
dimensions. In that case, the driver will append the coverage metadata
and show zero bands. At that point, the user must use options to further
instruct the driver how to deal with extra dimensions and data fields.

The WCS driver supports WCS versions 1.0.0, 1.1.0, 1.1.1, 1.1.2, and
2.0.1 at basic level (version 0.7 is not supported and support for
version 2.0.1 is available starting at GDAL 2.3). Any return format that
is a single file, and is in a format supported by GDAL should work. The
driver will prefer a format with "tiff" in the name, otherwise it will
fallback to the first offered format. However, the user may set the
preferred format. Coordinate systems are read from the DescribeCoverage
result.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Service description file
------------------------

The service description file has the following elements as immediate
children of the document element. Note that when the "WCS:<URL>" syntax
for dataset name is used, the contents of the service description file
is meant to be modified by using options.

-  **ServiceURL**: URL of the service without parameters
-  **Version**: The WCS version that is used in the communication. If
   the dataset name syntax WCS:URL is used the default is 2.0.1 and the
   server's response may change the user request, otherwise defaults to
   1.0.0. Versions 1.0.0, 1.1.0, 1.1.1, 1.1.2, and 2.0.1 are supported.
-  **CoverageName**: The coverage that is used for the dataset.
-  **Format**: The format to use for GetCoverage calls. If not set,
   selected by the driver. (WCS version 2.0)
-  **PreferredFormat**: The format to use for GetCoverage calls. If not
   set, selected by the driver. (WCS versions 1.0 and 1.1)
-  **Interpolation**: The interpolation method used when scaling. Should
   be one of the server supported values. (GDAL 2.3)
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
-  **Elements for controlling the range and domain:**
-  **Domain**: The axes that are used for the spatial dimensions. The
   default is to use the first two axes given by the server. The axis
   order swap may apply. Syntax: *axis_name,axis_name* A
   *field_name:field_name* in the list denotes a range of fields. (Used
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
-  **Elements for controlling the communication:**
-  **Timeout**: The timeout to use for remote service requests. If not
   provided, the libcurl default is used.
-  **UserPwd**: May be supplied with *userid:password* to pass a userid
   and password to the remote server.
-  **HttpAuth**: May be BASIC, NTLM or ANY to control the authentication
   scheme to be used.
-  **GetCoverageExtra**: An additional set of keywords to add to
   GetCoverage requests in URL encoded form. eg.
   "&RESAMPLE=BILINEAR&Band=1". Note that the extra parameters should
   not be known parameters (see below).
-  **DescribeCoverageExtra**: An additional set of keywords to add to
   DescribeCoverage requests in URL encoded form. eg. "&CustNo=775"
-  **Elements that may be needed to deal with server quirks (GDAL
   2.3):**
   **Note:** The options are not propagated to the subdataset with the
   switch -sd.
-  **OriginAtBoundary**: Set this flag if the server reports grid origin
   to be at the pixel boundary instead of the pixel center. (Use for
   MapServer versions <= 7.0.7 with WCS versions 1.0.0 and 2.0.1)
-  **OuterExtents**: Set to consider WCS 1.1 extents as boundaries of
   outer pixels instead of centers of outer pixels. (Use for GeoServer).
-  **BufSizeAdjust**: Set to 0.5 in WCS 1.1 if data access fails due to
   the response not having expected size. (Use for GeoServer).
-  **OffsetsPositive**: Use with MapServer in WCS version 2.0.1 together
   wwith NrOffsets.
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

Range and dimension subsetting
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When WCS version 2.0.1 is used, the range (fields/bands) and the
dimension can and/or may need to be subsetted. If the data model of the
coverage contains dimensions beyond the two geographic or map
coordinates, those dimensions must be sliced for GDAL. The coverage may
also contain a large number of fields, from which only a subset is
wanted in the GDAL dataset.

Range and dimension subsetting must be done via URL parameters since
from one coverage it is possible to create more than one different GDAL
datasets. In the WCS cache this means that there may be the sets of
files related to a GDAL dataset:

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
   dataset. Syntax: *field_name,field_name:field_name,..* (Note:
   requires that the server implements the range subsetting extension.)
-  **Subset**: Trim or slice a dimension when fetching data from the
   server. Syntax:
   *axis_name(trim_begin_value,trim_end_value);axis_name(slice_value)*
   Note that trimming the geographic/map coordinates is done by the
   driver.

Other WCS parameters
~~~~~~~~~~~~~~~~~~~~

The following WCS (version 2.0.1) parameters are recognized besides what
has been described above. These all can be set either through options or
directly into the URL. The ones in URL take precedence. Note that it is
up to the server to support/recognize these.

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

When the "WCS:<URL>" dataset name syntax is used, open options are used
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

When the "WCS:<URL>" dataset name syntax is used, the server responses,
the service description file, and the metadata files are stored in a
cache. Generally, if the needed resource is in the cache, it will be
used and no extra calls to the server are done.

The default location of the cache directory is $HOME/.gdal/wcs_cache

The cache contents can be seen as subdatasets using an empty URL:

::

   gdalinfo "WCS:"

The cache control options/flags are

-  **CACHE=path** Overrides the default cache location.
-  **CLEAR_CACHE** The cache is completely initialized and all files are
   deleted.
-  **REFRESH_CACHE** The cache entry, either capabilities or coverage,
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

A gdalinfo call to a coverage served by MapServer:

::

   gdalinfo \
   -oo INTERLEAVE=PIXEL \
   -oo OffsetsPositive \
   -oo NrOffsets=2 \
   -oo NoGridAxisSwap \
   -oo BandIdentifier=none \
   "WCS:http://194.66.252.155/cgi-bin/BGS_EMODnet_bathymetry/ows?VERSION=1.1.0&coverage=BGS_EMODNET_CentralMed-MCol"

A gdal_translate call to a scaled clip of a coverage served by
GeoServer:

::

   gdal_translate \
   -oo CACHE=wcs_cache \
   -oo CLEAR_CACHE \
   -oo INTERLEAVE=PIXEL \
   -projwin 377418 6683393.87938218 377717.879386966 6683094 \
   -oo Subset="time(1985-01-01T00:00:00.000Z)" \
   -outsize 60 0 \
   "WCS:https://beta-karttakuva.maanmittauslaitos.fi/wcs/service/ows?version=2.0.1&coverage=ortokuva__ortokuva" \
   scaled.tiff

See Also
--------

-  `OGC WCS Standards <http://www.opengeospatial.org/standards/wcs>`__
