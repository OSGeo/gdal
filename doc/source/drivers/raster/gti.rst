.. _raster.gti:

================================================================================
GTI -- GDAL Raster Tile Index
================================================================================

.. versionadded:: 3.9

.. shortname:: GTI

.. built_in_by_default::

Introduction
------------

The GTI driver is a driver that allows to handle catalogs with a large
number of raster files (called "tiles" in the rest of this document, even if a
regular tiling is not required by the driver), and build a virtual mosaic from
them. Each tile may be in any GDAL supported raster format, and be a file
stored on a regular filesystem, or any GDAL supported virtual filesystem (for
raster drivers that support such files).

This driver offers similar functionality as the :ref:`VRT <raster.vrt>`
driver with the following main differences:

* The tiles are listed as features of any GDAL supported vector format. Use of
  formats with efficient spatial filtering is recommended, such as
  :ref:`GeoPackage <vector.gpkg>`, :ref:`FlatGeoBuf <vector.flatgeobuf>` or
  :ref:`PostGIS <vector.pg>`. The GTI driver can thus use a larger number of
  tiles than the VRT driver (hundreds of thousands or more), provided the
  underlying vector format is efficient.

* The tiles may have different SRS. The GTI driver is capable of on-the-fly
  reprojection

* The GTI driver offers control on the order in which tiles are composited,
  when they overlap (z-order)

* The GTI driver honours the mask/alpha band when compositing together
  overlapping tiles.

* Contrary to the VRT driver, the GTI driver does not enable to alter
  characteristics of referenced tiles, such as their georeferencing, nodata value,
  etc. If such behavior is desired, the tiles must be for example wrapped
  individually in a VRT file before being referenced in the GTI index.

Connection strings
------------------

The GTI driver accepts different types of connection strings:

* a vector file in GeoPackage format with a ``.gti.gpkg`` extension, or in
  FlatGeoBuf format with a ``.gti.fgb`` extension, meeting the minimum requirements
  for a GTI compatible tile index, detailed later.

  For example: ``tileindex.gti.gpkg``

  Starting with GDAL 3.10, specifying the ``-if GTI`` option to command line utilities
  accepting it, or ``GTI`` as the only value of the ``papszAllowedDrivers`` of
  :cpp:func:`GDALOpenEx`, also forces the driver to recognize the passed filename
  if its extension is just ``.gpkg`` or ``.fgb``.

* any vector file in a GDAL supported format, with its filename (or connection
  string prefixed with ``GTI:``

  For example: ``GTI:tileindex.shp`` or ``GTI:PG:database=my_db schema=tileindex``

* a XML file, following the below GTI XML format, generally with the
  recommended ``.gti`` extension, referencing a vector file. Using such
  XML file may be more practical for tile indexes not stored in a file, or
  if some additional metadata must be defined at the dataset or band level of
  the virtual mosaic.

  For example: ``tileindex.gti``

STAC GeoParquet support
-----------------------

.. versionadded:: 3.10

The driver can support `STAC GeoParquet catalogs <https://stac-utils.github.io/stac-geoparquet/latest/spec/stac-geoparquet-spec>`_,
provided GDAL is build with :ref:`vector.parquet` support.
It can make use of fields ``proj:epsg`` and ``proj:transform`` from the
`Projection Extension Specification <https://github.com/stac-extensions/projection/>`_,
to correctly infer the appropriate projection and resolution.

Example of a valid connection string: ``GTI:/vsicurl/https://github.com/stac-utils/stac-geoparquet/raw/main/tests/data/naip.parquet``

Tile index requirements
-----------------------

The minimum requirements for a GTI compatible tile index is to be a
vector format supported by GDAL, with a geometry column storing polygons with
the extent of the tiles, and an attribute field of type string, storing the
path to each tile. The default name for this attribute field is ``location``.
If relative filenames are stored in the tile index, they are considered to
be relative to the path of the tile index.

In addition, for formats that can store layer metadata (GeoPackage, FlatGeoBuf,
PostGIS, ...), the following layer metadata items may be set:

* ``RESX=<float>`` and ``RESY=<float>``: resolution along X and Y axis,
  in SRS units / pixel.

  Setting those metadata items is recommended, otherwise
  the driver will try to open one of the tiles referenced in the tile index,
  and use its resolution as the resolution for the mosaic.

* ``BAND_COUNT=<int>``: number of bands of the virtual mosaic. The tiles
  stored in an index should generally have the same number of bands.

  Setting that metadata item is recommended, otherwise
  the driver will try to open one of the tiles referenced in the tile index, and
  use it as the number of bands for the mosaic.

  A mix of tiles with N and N+1 bands is allowed, provided that the color
  interpretation of the (N+1)th band is alpha. The N+1 value must be written
  as the band count in that situation.

  If tiles contains a single band with a color table, and that the color table
  may differ among tiles, BAND_COUNT should be set to 3 (resp. 4) to perform
  expansion to Red, Green, Blue components (resp. Red, Green, Blue, Alpha).
  If the color table is identical between the tiles, and it is desired to
  preserve it, the VRRTI XML file format may be used to include the ColorTable
  element.


* ``DATA_TYPE=<val>``: data type of the tiles of the tile index
  ``Byte``, ``Int8``, ``UInt16``,
  ``Int16``, ``UInt32``, ``Int32``, ``UInt64``, ``Int64``, ``Float32``, ``Float64``, ``CInt16``,
  ``CInt32``, ``CFloat32`` or ``CFloat64``

  Setting that metadata item is recommended, otherwise
  the driver will try to open one of the tiles referenced in the tile index, and
  use it as the data type for the mosaic.

* ``NODATA=<val>[,<val]...``: nodata value of the bands of the virtual mosaic.

  Note that source tiles may have or may not have a nodata value themselves,
  and it may be different than the nodata value of the virtual mosaic.

* ``MINX=<float>``, ``MINY=<float>``, ``MAXX=<float>`` and ``MAXY=<float>``:
  defines the extent of the virtual mosaic.

  For vector formats that have efficient retrieval of the layer extent, setting
  those items is not needed.

* ``GEOTRANSFORM=<gt0>,<gt1>,<gt2>,<gt3>,<gt4>,<gt5>``: defines the GeoTransform.
  Used together with ``XSIZE`` and ``YSIZE``, this is an alternate way of
  defining the extent and resolution os the virtual mosaic.

  It is not necessary to define this item if ``RESX=`` and ``RESY`` are set
  (potentially accompanied with ``MINX``, ``MINY``, ``MAXX`` and ``MAXY``)

* ``XSIZE=<int>``, ``YSIZE=<int>``: size of the virtual mosaic in pixel.

* ``COLOR_INTERPRETATION=<val>[,<val]...``: color interpretation of the bands
  of the mosaic. Possible values are ``red``, ``green``, ``blue``, ``alpha``,
  ``undefined``

* ``SRS=<string>``: defines the SRS of the virtual mosaic, using any value
  supported by the :cpp:func:`OGRSpatialReference::SetFromUserInput` call, which
  includes EPSG Projected, Geographic or Compound CRS (i.e. EPSG:4296), a
  well known text (WKT) CRS definition, PROJ.4 declarations, etc.

  It is not necessary to define this element if the virtual mosaic SRS is
  recorded as the SRS of the vector layer of the tile index.

* ``LOCATION_FIELD=<string>``: name of the field where the tile location is
  stored. Defaults to ``location``.

* ``SORT_FIELD=<string>``: name of a field to use to control the order in which
  tiles are composited, when they overlap (z-order). That field may be of
  type String, Integer, Integer64, Date or DateTime. By default, the higher the
  value in that field, the last the corresponding tile will be rendered in the
  virtual mosaic (unless SORT_FIELD_ASC=NO is set)

* ``SORT_FIELD_ASC=YES|NO``: whether the values in SORT_FIELD should be sorted
  in ascending or descending order. Defaults to YES (ascending)

* ``BLOCKXSIZE=<int>`` and ``BLOCKYSIZE=<int>``: Block size of bands of the
  virtual mosaic. Defaults to 256x256.

* ``MASK_BAND=YES|NO``: whether a dataset-level mask band should be exposed.
  Defaults to NO.

* ``RESAMPLING=<val>``: Resampling method to use for on-the-fly reprojection,
  or rendering of tiles whose origin coordinates are not at an offset multiple
  of the resolution of the mosaic compared to the origin of the mosaic. In that
  later case, RESAMPLING is only taken into account when requesting pixels with
  the default nearest resampling mode.

  Possible values: ``nearest``, ``cubic``, ``cubicspline``, ``lanczos``, ``average``, ``rms``, ``gauss``

  Defaults to ``nearest``

* ``BAND_<number>_OFFSET=<val>`` where number is an integer index starting at 1.

  Additive offset to apply to the raw numbers of the band.

* ``BAND_<number>_SCALE=<val>`` where number is an integer index starting at 1.

  Multiplicative factor to apply to the raw numbers of the band.

* ``BAND_<number>_UNITTYPE=<val>`` where number is an integer index starting at 1.

  Unit of the band.

* ``OVERVIEW_<idx>_DATASET=<string>`` where idx is an integer index (starting at 0
  since GDAL 3.9.2, starting at 1 in GDAL 3.9.0 and 3.9.1)

  Name of the dataset to use as the first overview level. This may be a
  raster dataset (for example a GeoTIFF file, or another GTI dataset).
  This may also be a vector dataset with a GTI compatible layer, potentially
  specified with ``OVERVIEW_<idx>_LAYER``.

  Starting with GDAL 3.9.2, overviews of ``OVERVIEW_<idx>_DATASET=<string>``
  are also automatically added, unless ``OVERVIEW_<idx>_OPEN_OPTIONS=OVERVIEW_LEVEL=NONE``
  is specified.

* ``OVERVIEW_<idx>_OPEN_OPTIONS=<key1=value1>[,key2=value2]...`` where idx is an integer index (starting at 0
  since GDAL 3.9.2, starting at 1 in GDAL 3.9.0 and 3.9.1)

  Open options(s) to use to open ``OVERVIEW_<idx>_DATASET``.

* ``OVERVIEW_<idx>_LAYER=<string>`` where idx is an integer index (starting at 0
  since GDAL 3.9.2, starting at 1 in GDAL 3.9.0 and 3.9.1)

  Only taken into account if ``OVERVIEW_<idx>_DATASET=<string>`` is not specified,
  or points to a GTI dataset.

  Name of the vector layer to use as the first overview level, assuming
  ``OVERVIEW_<idx>_DATASET`` points to a vector dataset. ``OVERVIEW_<idx>_DATASET``
  may also not be specified, in which case the vector dataset of the full
  resolution virtual mosaic is used.

* ``OVERVIEW_<idx>_FACTOR=<int>`` where idx is an integer index (starting at 0
  since GDAL 3.9.2, starting at 1 in GDAL 3.9.0 and 3.9.1)

  Sub-sampling factor, strictly greater than 1.

  Only taken into account if ``OVERVIEW_<idx>_DATASET=<string>`` is not specified,
  or points to a GTI dataset.

  If ``OVERVIEW_<idx>_DATASET`` and ``OVERVIEW_<idx>_LAYER`` are not specified, then all tiles of the full
  resolution virtual mosaic are used, with the specified sub-sampling factor
  (it is recommended, but not required, that those tiles do have a corresponding overview).
  ``OVERVIEW_<idx>_DATASET`` and/or ``OVERVIEW_<idx>_LAYER`` may also be
  specified to point to another tile index.

All overviews *must* have exactly the same extent as the full resolution
virtual mosaic. The GTI driver does not check that, and if that condition is
not met, subsampled pixel request will lead to incorrect result.

They also must be listed by decreasing size with increasing overview index.

In addition to those layer metadata items, the dataset-level metadata item
``TILE_INDEX_LAYER`` may be set to indicate, for dataset with multiple layers,
which one should be used as the tile index layer.

Alternatively to setting those metadata items individually, the corresponding
information can be grouped together in a GTI XML document, attached in the
``xml:GTI`` metadata domain of the layer (for drivers that support alternate
metadata domains such as GeoPackage)

GTI XML format
----------------

A `XML schema of the GDAL GTI format <https://raw.githubusercontent.com/OSGeo/gdal/master/data/gdaltileindex.xsd>`_
is available.

The following artificial example contains all potential elements and attributes.
A number of them have similar name and same semantics as layer metadata items
mentioned in the previous section.

.. code-block:: xml

    <GDALTileIndexDataset>
        <IndexDataset>PG:dbname=my_db</IndexDataset>   <!-- required for stanalone XML GTI files. Ignored if embedded in the xml:GTI metadata domain of the layer  -->
        <IndexLayer>my_layer</IndexLayer>              <!-- optional, but required if there are multiple layers in IndexDataset -->
        <Filter>pub_date >= '2023/12/01'</Filter>      <!-- optional -->
        <SortField>pub_date</SortField>                <!-- optional -->
        <SortFieldAsc>true</SortFieldAsc>              <!-- optional -->
        <SRS>EPSG:4326</SRS>                           <!-- optional -->
        <ResX>60</ResX>                                <!-- optional, but recommended -->
        <ResY>60</ResY>                                <!-- optional, but recommended -->
        <MinX>0</MinX>                                 <!-- optional -->
        <MinY>1</MinY>                                 <!-- optional -->
        <MaxX>2</MaxX>                                 <!-- optional -->
        <MaxY>3</MaxY>                                 <!-- optional -->
        <GeoTransform>2,1,0,49,0,-1</GeoTransform>     <!-- optional -->
        <XSize>2048</XSize>                            <!-- optional -->
        <YSize>1024</YSize>                            <!-- optional -->
        <BlockXSize>256</BlockXSize>                   <!-- optional -->
        <BlockYSize>256</BlockYSize>                   <!-- optional -->
        <Resampling>Cubic</Resampling>                 <!-- optional -->
        <BandCount>1</BandCount>                       <!-- optional, not needed if Band elements are defined -->

        <!-- Band is optional, but recommended. Repeated as many times as there are bands -->
        <!-- The "band" attribute is required -->
        <!-- The "dataType" attribute is optional, but recommended -->
        <Band band="1" dataType="Byte">
            <Description>my band</Description>         <!-- optional -->
            <Offset>2</Offset>                         <!-- optional -->
            <Scale>3</Scale>                           <!-- optional -->
            <NoDataValue>4</NoDataValue>               <!-- optional -->
            <UnitType>dn</UnitType>                    <!-- optional -->
            <ColorInterp>Gray</ColorInterp>            <!-- optional -->
            <ColorTable>                               <!-- optional -->
                <Entry c1="1" c2="2" c3="3" c4="255"/>
            </ColorTable>
            <CategoryNames>                            <!-- optional -->
                <Category>cat</Category>
            </CategoryNames>
            <GDALRasterAttributeTable><!--... --></GDALRasterAttributeTable>  <!-- optional -->
            <Metadata>                                 <!-- optional -->
                <MDI key="FOO">BAR</MDI>
            </Metadata>
            <Metadata domain="other_domain">           <!-- optional -->
                <MDI key="FOO">BAR</MDI>
            </Metadata>
        </Band>

        <Metadata>                                     <!-- optional -->
            <MDI key="FOO">BAR</MDI>
        </Metadata>
        <Metadata domain="other_domain">               <!-- optional -->
            <MDI key="FOO">BAR</MDI>
        </Metadata>

        <Overview>                                     <!-- optional -->
            <!-- 1st overview level will reuse the tile index of the
                 IndexDataset and IndexLayer elements, with all tiles considered
                 downsampled by a factor of 2 -->
            <Factor>2</Factor>
        </Overview>
        <Overview>                                     <!-- optional -->
            <!-- 2nd overview level will reuse the tile index of the
                 IndexDataset and IndexLayer elements, with all tiles considered
                 downsampled by a factor of 4 -->
            <Factor>4</Factor>
        </Overview>
        <Overview>                                     <!-- optional -->
            <!-- 3rd overview level (and potentially 4th, 5th... depending on
                 the number of overview levels in the pointed GeoTIFF file.
                 Only since GDAL 3.9.2)
            -->
            <Dataset>some.tif</Dataset>
        </Overview>
        <Overview>                                     <!-- optional -->
            <!-- Last overview level points to another GTI dataset -->
            <Dataset>other.gti.gpkg</Dataset>
            <Layer>other_layer</Layer>
            <OpenOptions>                              <!-- optional -->
                <OOI key="XMIN">0</OOI>
                <OOI key="YMIN">1</OOI>
                <OOI key="XMAX">2</OOI>
                <OOI key="YMAX">3</OOI>
            </OpenOptions>
        </Overview>

    </GDALTileIndexDataset>


At the GDALTileIndexDataset level, the elements specific to GTI XML are:

* ``Filter``: value of a SQL WHERE clause, used to select a subset of the
  features of the index.

* ``BlockXSize`` / ``BlockYSize``: dimension of the block size of bands.
  Defaults to 256x256

* ``Metadata``: defines dataset-level metadata. You can refer to the
  documentation of the :ref:`VRT <raster.vrt>` driver for its syntax.

At the Band level, the elements specific to GTI XML are: Description,
Offset, Scale, UnitType, ColorTable, CategoryNames, GDALRasterAttributeTable,
Metadata.
You can refer to the documentation of the :ref:`VRT <raster.vrt>` driver for
their syntax and semantics.


How to build a GTI compatible index ?
----------------------------------------

The :ref:`gdaltindex` program may be used to generate both a vector tile index,
and optionally a wrapping .gti XML file.

A GTI compatible index may also be created by any programmatic means, provided
the above format specifications are met.


Open options
------------

|about-open-options|
The following open options are available. Most of them can be
also defined as layer metadata items or in the .gti XML file


-  .. oo:: LAYER
      :choices: <string>

      For dataset with multiple layers, indicates which one should be used as
      the tile index layer.
      Same role as the TILE_INDEX_LAYER dataset level metadata item


-  .. oo:: LOCATION_FIELD
      :choices: <string>
      :default: location

      Name of the field where the tile location is stored.


-  .. oo:: SORT_FIELD
      :choices: <string>

      Name of a field to use to control the order in which
      tiles are composited, when they overlap (z-order). That field may be of
      type String, Integer, Integer64, Date or DateTime. By default, the higher the
      value in that field, the last the corresponding tile will be rendered in the
      virtual mosaic (unless SORT_FIELD_ASC=NO is set)

-  .. oo:: SORT_FIELD_ASC
      :choices: YES, NO
      :default: YES

      Whether the values in SORT_FIELD should be sorted in ascending or descending order

-  .. oo:: FILTER
      :choices: <string>

      Value of a SQL WHERE clause, used to select a subset of the features of the index.

-  .. oo:: RESX
      :choices: <float>

      Resolution along X axis in SRS units / pixel.

-  .. oo:: RESY
      :choices: <float>

      Resolution along Y axis in SRS units / pixel.

-  .. oo:: MINX
      :choices: <float>

      Minimum X value for the virtual mosaic extent

-  .. oo:: MINY
      :choices: <float>

      Minimum Y value for the virtual mosaic extent

-  .. oo:: MAXX
      :choices: <float>

      Maximum X value for the virtual mosaic extent

-  .. oo:: MAXY
      :choices: <float>

      Maximum Y value for the virtual mosaic extent

Multi-threading optimizations
-----------------------------

Starting with GDAL 3.10, the :oo:`NUM_THREADS` open option can
be set to control specifically the multi-threading of GTI datasets.
It defaults to ``ALL_CPUS``, and when set, overrides :config:`GDAL_NUM_THREADS`
or :config:`GTI_NUM_THREADS`. It applies to band-level and dataset-level
RasterIO(), if more than 1 million pixels are requested and if the mosaic is
made of only non-overlapping tiles.

-  .. oo:: NUM_THREADS
      :choices: integer, ALL_CPUS
      :default: ALL_CPUS

      Determines the number of threads used when an operation reads from
      multiple sources.

This can also be specified globally with the :config:`GTI_NUM_THREADS`
configuration option.

-  .. config:: GTI_NUM_THREADS
      :choices: integer, ALL_CPUS
      :default: ALL_CPUS

      Determines the number of threads used when an operation reads from
      multiple sources.

Note that the number of threads actually used is also limited by the
:config:`GDAL_MAX_DATASET_POOL_SIZE` configuration option.
