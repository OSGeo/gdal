.. _raster.isis3:

================================================================================
ISIS3 -- USGS Astrogeology ISIS Cube (Version 3)
================================================================================

.. shortname:: ISIS3

.. built_in_by_default::

ISIS3 is a format used by the USGS Planetary Cartography group to store
and distribute planetary imagery data. GDAL provides
read/creation/update access to ISIS3 formatted imagery data.

ISIS3 files often have the extension .cub, sometimes with an associated
.lbl (label) file. When a .lbl file exists it should be used as the
dataset name rather than the .cub file. Since GDAL 2.2, the driver also
supports imagery stored in a separate GeoTIFF file.

In addition to support for most ISIS3 imagery configurations, this
driver also reads georeferencing and coordinate system information as
well as selected other header metadata.

Starting with GDAL 2.2, a mask band is attached to each source band. The
value of this mask band is 0 when the pixel value is the NULL value or
one of the low/high on-intstrument/processed saturation value, or 255
when the pixel value is valid.

Implementation of this driver was supported by the United States
Geological Survey.

ISIS3 is part of a family of related formats including PDS and ISIS2.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Metadata
--------

Starting with GDAL 2.2, the ISIS3 label can be retrieved as
JSon-serialized content in the json:ISIS3 metadata domain.

For example:

::

   $ python
   from osgeo import gdal
   ds = gdal.Open('../autotest/gdrivers/data/isis3_detached.lbl')
   print(ds.GetMetadata_List('json:ISIS3')[0])
   {
     "IsisCube":{
       "_type":"object",
       "Core":{
         "_type":"object",
         "StartByte":1,
         "^Core":"isis3_detached.cub",
         "Format":"BandSequential",
         "Dimensions":{
           "_type":"group",
           "Samples":317,
           "Lines":30,
           "Bands":1
         },
         "Pixels":{
           "_type":"group",
           "Type":"UnsignedByte",
           "ByteOrder":"Lsb",
           "Base":0.000000,
           "Multiplier":1.000000
         }
       },
       "Instrument":{
         "_type":"group",
         "TargetName":"Mars"
       },
       "BandBin":{
         "_type":"group",
         "Center":1.000000,
         "OriginalBand":1
       },
       "Mapping":{
         "_type":"group",
         "ProjectionName":"Equirectangular",
         "CenterLongitude":184.412994,
         "TargetName":"Mars",
         "EquatorialRadius":{
           "value":3396190.000000,
           "unit":"meters"
         },
         "PolarRadius":{
           "value":3376200.000000,
           "unit":"meters"
         },
         "LatitudeType":"Planetographic",
         "LongitudeDirection":"PositiveWest",
         "LongitudeDomain":360,
         "MinimumLatitude":-14.822815,
         "MaximumLatitude":-14.727503,
         "MinimumLongitude":184.441132,
         "MaximumLongitude":184.496521,
         "UpperLeftCornerX":-4766.964984,
         "UpperLeftCornerY":-872623.628822,
         "PixelResolution":{
           "value":10.102500,
           "unit":"meters\/pixel"
         },
         "Scale":{
           "value":5864.945312,
           "unit":"pixels\/degree"
         },
         "CenterLatitude":-15.147000,
         "CenterLatitudeRadius":3394813.857978
       }
     },
     "Label":{
       "_type":"object",
       "Bytes":65536,
     },
     "History":{
       "_type":"object",
       "Name":"IsisCube",
       "StartByte":1,
       "Bytes":957,
       "^History":"r0200357_10m_Jul20_o_i3_detatched.History.IsisCube"
     },
     "OriginalLabel":{
       "_type":"object",
       "Name":"IsisCube",
       "StartByte":1,
       "Bytes":2482,
       "^OriginalLabel":"r0200357_10m_Jul20_o_i3_detatched.OriginalLabel.IsisCube"
     }
   }

or

::

   $ gdalinfo -json ../autotest/gdrivers/data/isis3_detached.lbl -mdd all

On creation, a source template label can be passed to the SetMetadata()
interface in the "json:ISIS3" metadata domain.

Creation support
----------------

Starting with GDAL 2.2, the ISIS3 driver supports updating imagery of
existing datasets, creating new datasets through the CreateCopy() and
Create() interfaces.

When using CreateCopy(), gdal_translate or gdalwarp, an effort is made
to preserve as much as possible of the original label when doing ISIS3
to ISIS3 conversions. This can be disabled with the USE_SRC_LABEL=NO
creation option.

The available creation options are:

-  **DATA_LOCATION**\ =LABEL/EXTERNAL/GEOTIFF. To specify the location
   of pixel data. The default value is LABEL, ie imagery immediately
   follows the label. If using EXTERNAL, the imagery is put in a raw
   file whose filename is the main filename with a .cub extension. If
   using GEOTIFF, the imagery is put in a separate GeoTIFF file, whose
   filename is the main filename with a .tif extension.
-  **GEOTIFF_AS_REGULAR_EXTERNAL**\ =YES/NO. Whether the GeoTIFF file,
   if uncompressed, should be registered as a regular raw file. Defaults
   to YES, so as to maximimze the compatibility with earlier version of
   the ISIS3 driver.
-  **GEOTIFF_OPTIONS**\ =string. Comma separated list of KEY=VALUE
   tuples to forward to the GeoTIFF driver. e.g.
   GEOTIFF_OPTIONS=COMPRESS=LZW.
-  **EXTERNAL_FILENAME**\ =filename. Override default external filename.
   Only for DATA_LOCATION=EXTERNAL or GEOTIFF.
-  **TILED**\ =YES/NO. Whether the pixel data should be tiled. Default
   is NO (ie band sequential organization).
-  **BLOCKXSIZE**\ =int_value. Tile width in pixels. Only used if
   TILED=YES. Defaults to 256.
-  **BLOCKYSIZE**\ =int_value. Tile height in pixels. Only used if
   TILED=YES. Defaults to 256.
-  **COMMENT**\ =string. Comment to add into the label.
-  **LATITUDE_TYPE**\ =Planetocentric/Planetographic. Value of
   Mapping.LatitudeType. Defaults to Planetocentric. If specified, and
   USE_SRC_MAPPING is in effect, this will be taken into account to
   override the source LatitudeType.
-  **LONGITUDE_DIRECTION**\ =PositiveEast/PositiveWest. Value of
   Mapping.LongitudeDirection. Defaults to PositiveEast. If specified,
   and USE_SRC_MAPPING is in effect, this will be taken into account to
   override the source LongitudeDirection.
-  **TARGET_NAME**\ =string. Value of Mapping.TargetName. This is
   normally deduced from the SRS datum name. If specified, and
   USE_SRC_MAPPING is in effect, this will be taken into account to
   override the source TargetName.
-  **FORCE_360**\ =YES/NO. Whether to force longitudes in the [0, 360]
   range. Defaults to NO.
-  **WRITE_BOUNDING_DEGREES**\ =YES/NO. Whether to write
   Min/MaximumLong/ Latitude values. Defaults to YES.
-  **BOUNDING_DEGREES**\ =min_long,min_lat,max_long,max_lat. Manually
   set bounding box (values will not be modified by LONGITUDE_DIRECTION
   or FORCE_360 options).
-  **USE_SRC_LABEL**\ =YES/NO. Whether to use source label in ISIS3 to
   ISIS3 conversions. Defaults to YES.
-  **USE_SRC_MAPPING**\ =YES/NO. Whether to use Mapping group from
   source label in ISIS3 to ISIS3 conversions. Defaults to NO (that is
   to say that the content of Mapping group will be created from new
   dataset geotransform and projection). Only used if USE_SRC_LABEL=YES
-  **USE_SRC_HISTORY**\ =YES/NO. Whether to use the content pointed by
   the source History object in ISIS3 to ISIS3 conversions, and write it
   to the new dataset. Defaults to YES. Only used if USE_SRC_LABEL=YES.
   If ADD_GDAL_HISTORY and USE_SRC_HISTORY are set to YES (or
   unspecified), a new history section will be appended to the existing
   history.
-  **ADD_GDAL_HISTORY**\ =YES/NO. Whether to add GDAL specific history
   in the content pointed by the History object in ISIS3 to ISIS3
   conversions. Defaults to YES. Only used if USE_SRC_LABEL=YES. If
   ADD_GDAL_HISTORY and USE_SRC_HISTORY are set to YES (or unspecified),
   a new history section will be appended to the existing history. When
   ADD_GDAL_HISTORY=YES, the history is normally composed from current
   GDAL version, binary name and path, host name, user name and source
   and target filenames. It is possible to completely override it by
   specifying the GDAL_HISTORY option.
-  **GDAL_HISTORY**\ =string. Manually defined GDAL history. Must be
   formatted as ISIS3 PDL. If not specified, it is automatically
   composed. Only used if ADD_GDAL_HISTORY=YES (or unspecified).

Examples
--------

How to create a copy of a source ISIS3 dataset to another ISIS3 dataset
while modifying a parameter of IsisCube.Mapping group, by using GDAL
Python :

::

   import json
   from osgeo import gdal

   src_ds = gdal.Open('in.lbl')
   # Load source label as JSon
   label = json.loads( src_ds.GetMetadata_List('json:ISIS3')[0] )
   # Update parameter
   label["IsisCube"]["Mapping"]["TargetName"] = "Moon"

   # Instantiate new raster
   # Note the USE_SRC_MAPPING=YES creation option, since we modified the
   # IsisCube.Mapping section, which otherwise is completely rewritten from
   # the geotransform and projection attached to the output dataset.
   out_ds = gdal.GetDriverByName('ISIS3').Create('out.lbl',
                                                 src_ds.RasterXSize,
                                                 src_ds.RasterYSize,
                                                 src_ds.RasterCount,
                                                 src_ds.GetRasterBand(1).DataType,
                                                 options = ['USE_SRC_MAPPING=YES'])
   # Attach the modified label
   out_ds.SetMetadata( [json.dumps(label)], 'json:ISIS3' )

   # Copy imagery (assumes that each band fits into memory, otherwise a line-by
   # line or block-per-block strategy would be more appropriate )
   for i in range(src_ds.RasterCount):
       out_ds.GetRasterBand(1).WriteRaster( 0, 0,
                                           src_ds.RasterXSize,
                                           src_ds.RasterYSize,
                                           src_ds.GetRasterBand(1).ReadRaster() )
   out_ds = None
   src_ds = None

See Also
--------

-  Implemented as ``gdal/frmts/pds/isis3dataset.cpp``.
-  :ref:`GDAL PDS Driver <raster.pds>`
-  :ref:`GDAL ISIS2 Driver <raster.isis2>`
