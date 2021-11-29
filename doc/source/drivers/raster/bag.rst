.. _raster.bag:

================================================================================
BAG -- Bathymetry Attributed Grid
================================================================================

.. shortname:: BAG

.. build_dependencies:: libhdf5

This driver provides read-only support, and starting with GDAL 2.4 for
creation, for bathymetry data in the BAG format. BAG files are actually
a specific product profile in an HDF5 file, but a custom driver exists
to present the data in a more convenient manner than is available
through the generic HDF5 driver.

BAG files have two image bands representing Elevation (band 1),
Uncertainty (band 2) values for each cell in a raster grid area.
Nominal Elevation values may also be present, and starting with GDAL 3.2, any
2D array of numeric data type and with the same dimensions as Elevation present
under BAG_root will be reported as a GDAL band.

The geotransform and coordinate system is extracted from the internal
XML metadata provided with the dataset. However, some products may have
unsupported coordinate system formats, if using the non-WKT way of
encoding the spatial reference system.

The full XML metadata is available in the "xml:BAG" metadata domain.

Nodata, minimum and maximum values for each band are also reported.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

    This driver supports the :cpp:func:`GDALDriver::Create` operation

    .. versionadded:: 3.2

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

For open options specific to variable resolution, see following chapter.

Other open options are:

- REPORT_VERTCRS=YES/NO (starting with GDAL 3.2). Defaults to YES. To report
  the vertical CRS from BAG XML metadata as the vertical component of a
  compound CRS. If set to NO, only the horizontal part will be reported.

Variable resolution (VR) grid support
-------------------------------------

Starting with GDAL 2.4, GDAL can handle BAG files with `variable
resolution
grids <https://bitbucket.org/ccomjhc/openns/raw/master/docs/VariableResolution/2017-08-10_VariableResolution.docx>`__.
Such datasets are made of a low-resolution grid, which is the one
presented by default by the driver, and for each of those low-resolution
cells, a higher resolution grid can be present in the file. Such higher
resolution grids are dubbed "supergrids" in GDAL.

The driver has different modes of working which can be controlled by the
MODE open option:

-  MODE=LOW_RES_GRID: this is the default mode. The driver will expose
   the low resolution grid, and indicate in the dataset metadata if the
   dataset has supergrids (HAS_SUPERGRIDS=TRUE), as well as the minimum
   and maximum resolution of those grids.
-  MODE=LIST_SUPERGRIDS: in this mode, the driver will report the
   various supergrids in the subdataset list. It is possible to apply in
   this mode additional open options to restrict the search

   -  SUPERGRIDS_INDICES=(y1,x1),(y2,x2),...: Tuple or list of tuples,
      of supergrids described by their y,x indices (starting from 0, y
      from the south of the grid, x from the west o the grid).
   -  MINX=value: Minimum georeferenced X value to use as a filter for
      the supergrids to list.
   -  MINY=value: Minimum georeferenced Y value to use as a filter for
      the supergrids to list.
   -  MAXX=value: Maximum georeferenced X value to use as a filter for
      the supergrids to list.
   -  MAXY=value: Maximum georeferenced Y value to use as a filter for
      the supergrids to list.
   -  RES_FILTER_MIN=value: Minimum resolution of supergrids to take
      into account (excluded bound)
   -  RES_FILTER_MAX=value: Maximum resolution of supergrids to take
      into account (included bound)

-  Opening a supergrid. This mode is triggered by using as a dataset
   name a string formatted like BAG:my.bag:supergrid:{y}:{x}, which is
   the value of the SUBDATASET_x_NAME metadata items reported by the
   above described mode. {y} is the index (starting from 0, from the
   south of the grid), and {x} is the index (starting from 0, from the
   west of the grid) of the supergrid to open.
-  MODE=RESAMPLED_GRID: in this mode, the user specify the extent and
   resolution of a target grid, and for each cell of this target grid,
   the driver will find the nodes of the supergrids that fall into that
   cell. By default, it will select the node with the maximum elevation
   value to populate the cell value. Or if no node of any supergrid are
   found, the cell value will be set to the nodata value.
   Overviews are reported: note that, those
   overviews correspond to resampled grids computed with different
   values of the RESX and RESY parameters, but using the same value
   population rules (and not nearest neighbour resampling of the full
   resolution resampled grid).

   The available open options in this mode are:

   -  MINX=value: Minimum georeferenced X value for the resampled grid.
      By default, the corresponding value of the low resolution grid.
   -  MINY=value: Minimum georeferenced Y value for the resampled grid.
      By default, the corresponding value of the low resolution grid.
   -  MAXX=value: Maximum georeferenced X value for the resampled grid.
      By default, the corresponding value of the low resolution grid.
   -  MAXY=value: Maximum georeferenced Y value for the resampled grid.
      By default, the corresponding value of the low resolution grid.
   -  RESX=value: Horizontal resolution. By default, and if RES_STRATEGY
      is set to AUTO, this will be the minimum resolution among all the
      supergrids.
   -  RESY=value: Vertical resolution (positive value). By default, and
      if RES_STRATEGY is set to AUTO, this will be the minimum
      resolution among all the supergrids.
   -  RES_STRATEGY=AUTO/MIN/MAX/MEAN: Which strategy to apply to set the
      resampled grid resolution. By default, if none of RESX, RESY,
      RES_FILTER_MIN and RES_FILTER_MAX is specified, the AUTO strategy
      will correspond to the MIN strategy: that is the minimum
      resolution among all the supergrids is used. If MAX is specified,
      the maximum resolution among all the supergrids is used. If MEAN
      is specified, the mean resolution among all the supergrids is
      used. RESX and RESY, if defined, will override the resolution
      determined by RES_STRATEGY.
   -  RES_FILTER_MIN=value: Minimum resolution of supergrids to take
      into account (excluded bound, except if it is the minimum
      resolution of supergrids). By default, the minimum resolution of
      supergrids available. If this value is specified and none of
      RES_STRATEGY, RES_FILTER_MAX, RESX or RESY is specified, the
      maximum resolution among all the supergrids will be used as the
      resolution for the resampled grid.
   -  RES_FILTER_MAX=value: Maximum resolution of supergrids to take
      into account (included bound). By default, the maximum resolution
      of supergrids available. If this value is specified and none of
      RES_STRATEGY, RESX or RESY is specified, this will also be used as
      the resolution for the resampled grid.
   -  VALUE_POPULATION=MIN/MAX/MEAN/COUNT: Which value population strategy to
      apply to compute the resampled cell values. This default to MAX:
      the elevation value of a target cell is the maximum elevation of
      all supergrid nodes (potentially filtered with RES_FILTER_MIN
      and/or RES_FILTER_MAX) that fall into this cell; the corresponding
      uncertainty will be the uncertainty of the source node where this
      maximum elevation si reached. If no supergrid node fall into the
      target cell, the nodata value is set. The MIN strategy is similar,
      except that this is the minimum elevation value among intersecting
      nodes that is selected. The MEAN strategy uses the mean value of
      the elevation of intersecting nodes, and the maximum uncertainty
      of those nodes.
      The COUNT strategy (GDAL >= 3.2) exposes one single UInt32 band where
      each target cell contains the count of supergrid nodes that fall into it.
   -  SUPERGRIDS_MASK=YES/NO. Default to NO. If set to YES, instead of
      the elevation and uncertainty band, the dataset contains a single
      Byte band which is boolean valued. For a target cell, if at least
      one supergrids nodes (potentially filtered with RES_FILTER_MIN
      and/or RES_FILTER_MAX) falls into the cell, the cell value is set
      at 255. Otherwise it is set at 0. This can be used to distinguish
      if elevation values at nodata are due to no source supergrid node
      falling into them, or if that/those supergrid nodes were
      themselves at the nodata value.
   -  NODATA_VALUE=value. Override the default value, which is usually
      1000000.

Spatial metadata support
------------------------

Starting with GDAL 3.2, GDAL can expose BAG files with `spatial metadata
<https://github.com/OpenNavigationSurface/BAG/issues/2>`__.

When such spatial metadata is present, the subdataset list will include
names of the form 'BAG:"{filename}":georef_metadata:{name_of_layer}'
where ``name_of_layer`` is the name of a subgroup under ``/BAG_root/Georef_metadata``

The values of the ``keys`` dataset under each metadata layer are used as the
GDAL raster value. And the corresponding ``values`` dataset is exposed as a
GDAL Raster Attribute Table associated to the GDAL raster band. If ``keys``
is absent, record 1 of ``values`` is assumed to be met for each elevation point
that does not match the nodata value of the elevation band.

When variable resolution grids are present, the MODE=LIST_SUPERGRIDS open option
will cause subdatasets of names of the form 'BAG:"{filename}":georef_metadata:{name_of_layer}:{y}:{x}'
to be reported. When opening such a subdataset, the ``varres_keys`` dataset will
be used to populate the GDAL raster value.
If ``varres_keys`` is absent, record 1 of ``values`` is assumed to be met for
each elevation point that does not match the nodata value of the variable resolution
elevation band.

Tracking list support
---------------------

When the dataset is opened in vector mode (ogrinfo, ogr2ogr, etc.), the tracking_list
dataset will be reported as a OGR vector layer

Creation support
----------------

Starting with GDAL 2.4, the driver can create a BAG dataset (without
variable resolution extension) with the elevation and uncertainty bands
from a source dataset. The source dataset must be georeferenced, and
have one or two bands. The first band is assumed to be the elevation
band, and the second band the uncertainty band. If the second band is
missing, the uncertainty will be set to nodata.

The driver will instantiate the BAG XML metadata by using a template
file, which is by default,
`bag_template.xml <https://raw.githubusercontent.com/OSGeo/gdal/master/data/bag_template.xml>`__,
found in the GDAL data definition files. This template contains
variables, present as ${KEYNAME} or ${KEYNAME:default_value} in the XML
file, that can be substituted by providing a creation option whose name
is the VAR\_ string prefixed to the key name. Currently those creation
options are:

-  VAR_INDIVIDUAL_NAME=string: to fill
   contact/CI_ResponsibleParty/individualName. If not provided, default
   to "unknown".
-  VAR_ORGANISATION_NAME=string: to fill
   contact/CI_ResponsibleParty/organisationName. If not provided,
   default to "unknown".
-  VAR_POSITION_NAME=string: to fill
   contact/CI_ResponsibleParty/positionName. If not provided, default to
   "unknown".
-  VAR_DATE=YYYY-MM-DD: to fill dateStamp/Date. If not provided, default
   to current date.
-  VAR_VERT_WKT=wkt_string: to fill
   referenceSystemInfo/MD_ReferenceSystem/referenceSystemIdentifier/RS_Identifier/code
   for the vertical coordinate reference system. If not provided, and if
   the input CRS is not a compound CRS, default to VERT_CS["unknown",
   VERT_DATUM["unknown", 2000]].
-  VAR_ABSTRACT=string: to fill identificationInfo/abstract. If not
   provided, default to empty string
-  VAR_PROCESS_STEP_DESCRIPTION=string: to fill
   dataQualityInfo/lineage/LI_Lineage/processStep/LI_ProcessStep/description.
   If not provided, default to "Generated by GDAL x.y.z".
-  VAR_DATETIME=YYYY-MM-DDTHH:MM:SS : to fill
   dataQualityInfo/lineage/LI_Lineage/processStep/LI_ProcessStep/dateTime/DateTime.
   If not provided, default to current datetime.
-  VAR_RESTRICTION_CODE=enumerated_value: to fill
   metadataConstraints/MD_LegalConstraints/useConstraints/MD_RestrictionCode.
   If not provided, default to "otherRestrictions".
-  VAR_OTHER_CONSTRAINTS=string: to fill
   metadataConstraints/MD_LegalConstraints/otherConstraints. If not
   provided, default to "unknown".
-  VAR_CLASSIFICATION=enumerated_value: to fill
   metadataConstraints/MD_SecurityConstraints/classification/MD_ClassificationCode.
   If not provided, default to "unclassified".
-  VAR_SECURITY_USER_NOTE=string: to fill
   metadataConstraints/MD_SecurityConstraints/userNote. If not provided,
   default to "none".

Other required variables found in the template, such as RES, RESX, RESY,
RES_UNIT, HEIGHT, WIDTH, CORNER_POINTS and HORIZ_WKT will be
automatically filled from the input dataset metadata.

The other following creation options are available:

-  TEMPLATE=filename: Path to a XML file that can serve as a template.
   This will typically be a customized version of the base
   bag_template.xml file. The file can contain other substituable
   variables than the ones mentioned above by using a similar syntax.
-  VAR_xxxx=value: Substitute variable ${xxxx} in the template XML value
   by the provided value.
-  BAG_VERSION=string: Value to write in the /BAG_root/BAG Version
   attribute. Default to 1.6.2.
-  COMPRESS=NONE/DEFLATE: Compression for elevation and uncertainty
   grids. Default to DEFLATE.
-  ZLEVEL=[1-9]: Deflate compression level. Defaults to 6.
-  BLOCK_SIZE=value_in_pixel: Chunking size of the HDF5 arrays. Default
   to 100, or the maximum dimension of the raster if smaller than 100.

Usage examples
--------------

-  Opening in low resolution mode:

   ::

      $ gdalinfo data/test_vr.bag

      [...]
      Size is 6, 4
      [...]
        HAS_SUPERGRIDS=TRUE
        MAX_RESOLUTION_X=29.900000
        MAX_RESOLUTION_Y=31.900000
        MIN_RESOLUTION_X=4.983333
        MIN_RESOLUTION_Y=5.316667
      [...]

-  Displaying available supergrids:

   ::

      $ gdalinfo data/test_vr.bag -oo MODE=LIST_SUPERGRIDS

      [...]
      Subdatasets:
        SUBDATASET_1_NAME=BAG:"data/test_vr.bag":supergrid:0:0
        SUBDATASET_1_DESC=Supergrid (y=0, x=0) from (x=70.100000,y=499968.100000) to (x=129.900000,y=500031.900000), resolution (x=29.900000,y=31.900000)
        SUBDATASET_2_NAME=BAG:"data/test_vr.bag":supergrid:0:1
        SUBDATASET_2_DESC=Supergrid (y=0, x=1) from (x=107.575000,y=499976.075000) to (x=152.424999,y=500023.924999), resolution (x=14.950000,y=15.950000)
      [...]
        SUBDATASET_24_NAME=BAG:"data/test_vr.bag":supergrid:3:5
        SUBDATASET_24_DESC=Supergrid (y=3, x=5) from (x=232.558335,y=500077.391667) to (x=267.441666,y=500114.608334), resolution (x=4.983333,y=5.316667)
      [...]

-  Opening a particular supergrid:

   ::

      $ gdalinfo BAG:"data/test_vr.bag":supergrid:3:5

-  Converting a BAG in resampling mode with default parameters (use of
   minimum resolution of supergrids, MAX value population rule):

   ::

      $ gdal_translate data/test_vr.bag -oo MODE=RESAMPLED_GRID out.tif

-  Converting a BAG in resampling mode with a particular grid origin and
   resolution

   ::

      $ gdal_translate data/test_vr.bag -oo MODE=RESAMPLED_GRID -oo MINX=80 -oo MINY=500000 -oo RESX=16 -oo RESY=16 out.tif

-  Converting a BAG in resampling mode, with a mask indicating where
   supergrids nodes intersect the cell of the resampled dataset.

   ::

      $ gdal_translate data/test_vr.bag -oo MODE=RESAMPLED_GRID -oo SUPERGRIDS_MASK=YES out.tif

-  Converting a BAG in resampling mode, by filtering on supergrid
   resolutions (and the resampled grid will use 4 meter resolution by
   default)

   ::

      $ gdal_translate data/test_vr.bag -oo MODE=RESAMPLED_GRID -oo RES_FILTER_MIN=4 -oo RES_FILTER_MAX=8 out.tif

-  Converting a GeoTIFF file to a BAG dataset, and provide a custom
   value for the ABSTRACT substituable variable.

   ::

      $ gdal_translate in.tif out.bag -co "VAR_ABSTRACT=My abstract"

-  Converting a (VR) BAG in resampling mode with a particular grid
   resolution (5m) to a BAG dataset (without variable resolution
   extension), and provide a custom value for the ABSTRACT metadata:

   ::

      $ gdal_translate data/test_vr.bag -oo MODE=RESAMPLED_GRID -oo RESX=5 -oo RESY=5 out.bag -co "VAR_ABSTRACT=My abstract"

-  Displaying the tracking list:

   ::

      $ ogrinfo -al data/my.bal

See Also
--------

-  Implemented as ``gdal/frmts/hdf5/bagdataset.cpp``.
-  `The Open Navigation Surface Project <http://www.opennavsurf.org>`__
-  `Description of Bathymetric Attributed Grid Object (BAG) Version
   1.6 <https://github.com/OpenNavigationSurface/BAG/raw/master/docs/BAG_FSD_Release_1.6.3.doc>`__
-  `Variable resolution grid extension for BAG
   files <https://github.com/OpenNavigationSurface/BAG/raw/master/docs/VariableResolution/2017-08-10_VariableResolution.docx>`__
