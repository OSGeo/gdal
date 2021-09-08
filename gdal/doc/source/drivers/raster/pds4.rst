.. _raster.pds4:

================================================================================
PDS4 -- NASA Planetary Data System (Version 4)
================================================================================

.. shortname:: PDS4

.. built_in_by_default::

PDS4 is a format used primarily by NASA to store and distribute solar,
lunar and planetary imagery data. GDAL provides read-write access to
PDS4 formatted imagery data.

PDS4 files are compose of a .xml (label) file which references a raw
imagery file. The driver also supports imagery stored in a separate
uncompressed GeoTIFF file with a strip organization compatible of a raw
imagery file.

The driver also reads and write georeferencing and coordinate system
information as well as selected other header metadata.

A mask band is attached to each source band. The value of this mask band
is 0 when the pixel value is one of the missing constants.

Implementation of this driver was supported by the United States
Geological Survey.

PDS4 is part of a family of related formats including PDS and ISIS3.

Starting with GDAL 2.5, the PDS4 driver supports reading and writing
ASCII fixed-with, binary fixed-with and delimited(CSV) tables as OGR
vector layers.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Metadata
--------

The PDS4 label can be retrieved as XML-serialized content in the
xml:PDS4 metadata domain.

On creation, a source template label can be passed to the SetMetadata()
interface in the "xml:PDS4" metadata domain.

Open options (vector only)
--------------------------

.. versionadded:: 3.0

When opening a PDS4 vector dataset, the following open options are
available:

-  **LAT**\ =string. Name of a field containing a Latitude value.
   Defaults to Latitude.
-  **LONG**\ =string. Name of a field containing a Longitude value.
   Defaults to Longitude.
-  **ALT**\ =string. Name of a field containing a Altitude value.
   Defaults to Altitude.
-  **WKT**\ =string. Name of a field containing a WKT value.
-  **KEEP_GEOM_COLUMNS**\ =YES/NO. Whether to expose original
   x/y/geometry columns as regular fields. Defaults to NO.

Creation support
----------------

The PDS4 driver supports updating imagery of existing datasets, creating
new datasets through the CreateCopy() and Create() interfaces.

When using CreateCopy(), gdal_translate or gdalwarp, an effort is made
to preserve as much as possible of the original label when doing PDS4 to
PDS4 conversions. This can be disabled with the USE_SRC_LABEL=NO
creation option.

The following dataset creation options are available:

-  Raster only:

   -  **IMAGE_FILENAME**\ =filename. Override default external image
      filename.
   -  **IMAGE_EXTENSION**\ =ext. Override default extension of the
      external image filename. The default is 'img' for IMAGE_FORMAT=RAW
      or 'tif' for IMAGE_FORMAT=GEOTIFF
   -  **IMAGE_FORMAT**\ =RAW/GEOTIFF. Format of the image file. If using
      RAW, the imagery is put in a raw file whose filename is the main
      filename with a .img extension. If using GEOTIFF, the imagery is
      put in a separate GeoTIFF file, whose filename is the main
      filename with a .tif extension. Defaults to RAW
   -  **INTERLEAVE**\ =BSQ/BIP/BIL. Pixel organization in the image
      file. BSQ is Band SeQuential, BIP is Band Interleaved per Pixel
      and BIL is Band Interleave Per Line. The default is BSQ. BIL is
      not valid for IMAGE_FORMAT=GEOTIFF
   -  **USE_SRC_LABEL**\ =YES/NO. Whether to use the source label in
      PDS4 to PDS4 conversions. Defaults to YES.
   -  **ARRAY_TYPE**\ =Array/Array_2D/Array_2D_Image/Array_2D_Map/
      Array_2D_Spectrum/Array_3D/Array_3D_Image/Array_3D_Movie/Array_3D_Spectrum.
      To set the XML element that defines the type of array. Defaults to
      Array_3D_Image. Using a Array_2D\* for a multiband image is not
      supported. When using a Array_2D\* value, INTERLEAVE will be
      ignored.
   -  **ARRAY_IDENTIFIER**\ =string. (GDAL >= 3.0) Identifier to put in
      the Array element.
   -  **UNIT**\ =string. (GDAL >= 3.0) Content of the
      Element_Array.unit. If not provided, the unit of the source band
      in case of copying from another raster will be used (if present on
      the source band).
   -  **CREATE_LABEL_ONLY**\ =YES/NO. (GDAL >= 3.1) If set to YES, and used
      in a gdal_translate / CreateCopy() context where the source dataset is
      a ENVI, GeoTIFF, ISIS3, VICAR, FITS or PDS3 dataset, whose layout is
      compatible of a raw binary format, as supported by PDS4, then only the
      label XML file will be generated, and it will reference the raw binary
      file of the source dataset. The IMAGE_FILENAME, IMAGE_FORMAT and
      INTERLEAVE creation options are ignored in that situation.

-  Raster and vector:

   -  **VAR_\***\ =string. If options like VAR_XXXX=yyyy are specified,
      any {XXXX} string in the template label will be replaced by the
      yyyy value.
   -  **TEMPLATE**\ =filename. Template label to use. If not specified
      and not creating from an existing PDS4 file, the
      data/pds4_template.xml file will be used. For GDAL utilities to
      find this default PDS4 template, GDAL's data directory should be
      defined in your environment (typically on Windows builds). Consult
      the
      `wiki <https://trac.osgeo.org/gdal/wiki/FAQInstallationAndBuilding#HowtosetGDAL_DATAvariable>`__
      for more information.
   -  **LATITUDE_TYPE**\ =Planetocentric/Planetographic. Value of
      latitude_type. Defaults to Planetocentric.
   -  **LONGITUDE_DIRECTION**\ =Positive East/Positive West. Value of
      longitude_direction. Defaults to Positive East.
   -  **RADII**\ =semi_major_radius,semi_minor_radius. To override the
      ones of the SRS. Note that the first value (semi_major_radius)
      will be used to set the <pds:semi_major_radius> and
      <pds:semi_minor_radius> XML elements, and that second value
      (semi_minor_radius) will be used to set the <pds:polar_radius> XML
      element.
   -  **BOUNDING_DEGREES**\ =west_lon,south_lat,east_lon,north_lat.
      Manually set bounding box

Layer creation options (vector/table datasets)
----------------------------------------------

(Starting with GDAL 3.0) When creating a PDS4 vector dataset, or
appending a new table to an existing table, the following layer creation
options are available:

-  **TABLE_TYPE**\ =DELIMITED/CHARACTER/BINARY. Determines the type of
   the PDS4 table to create. DELIMITED is the default and corresponds to
   a CSV table file (with comma field separator). CHARACTER corresponds
   to a fixed-width ASCII table. BINARY corresponds to a fixed-width
   table. For fixed-width table, for String fields, an arbitrary width
   of 64 bytes is used if there is no explicit field set in the OGR
   field definition. Only DELIMITED supports arbitrary encoding of
   geometry as a WKT string. The two other table types only support
   points for geographic coordinates (LAT, LONG).
-  **LINE_ENDING**\ = CRLF/LF. Determines the line-ending character sequence.
   Only applies to TABLE_TYPE=DELIMITED or CHARACTER. The default is CRLF
   (Carriage Return and Line Feed). It can be set to LF for just Line Feed
   character. (GDAL >= 3.4)
-  **GEOM_COLUMNS**\ =AUTO/WKT/LONG_LAT. Specify how the geometry is
   encoded. In AUTO mode, for DELIMITED tables, if the input geometry is
   Point with a geographic CRS attached to the laye, then a LONG and LAT
   columns will be created to store the point coordinates. For other
   geometry types, a WKT column is used. The WKT value of this option
   can also be used to force a WKT column to be created when a LONG and
   LAT columns would have been possible. For fixed-width table types,
   only AUTO and LONG_LAT are possible.
-  **CREATE_VRT**\ =YES/NO. Defaults to YES for a DELIMITED table. In
   that case, a OGR VRT (XML file) will be created along-side the .csv
   file.
-  **LAT**\ =string. Name of a field containing a Latitude value.
   Defaults to Latitude. Only used when the geometry comes from a Point
   layer with geographic CRS
-  **LONG**\ =string. Name of a field containing a Longitude value.
   Defaults to Longitude. Only used when the geometry comes from a Point
   layer with geographic CRS
-  **ALT**\ =string. Name of a field containing a Altitude value.
   Defaults to Altitude. Only used when the geometry comes from a Point
   layer with geographic CRS
-  **WKT**\ =string. Name of a field containing a WKT value.
-  **SAME_DIRECTORY**\ =YES/NO. Whether table files should be created in
   the same directory, or in a subdirectory. Defaults to NO, that is
   that table files will be created in a subdiretory whose name is the
   basename of the XML file. For example if creating a "foo.xml" PDS4
   dataset, table files will be created in the "foo" subdirectory by
   default. If this option is set to YES, they will be created in the
   same directory as "foo.xml".

Subdataset / multiple image support
-----------------------------------

If several Array objects are present in the label, they will be reported
as separate subdatasets (typically the main subdataset is an Array3D,
and backplanes are represented as Array2D).

Since GDAL 3.0, creation of new datasets with subdatasets is supported
(through the APPEND_SUBDATASET=YES creation option). One important
restriction is that, given that the georeferencing information in the
PDS4 XML label is global for the whole dataset, all subdatasets must
share the same georeferencing information: coordinate reference system,
georegistration and resolution. Appending to both RAW and GEOTIFF raster
is supported. In append mode, most creation options are ignored, except
INTERLEAVE (if GeoTIFF output image), ARRAY_TYPE and ARRAY_IDENTIFIER.

PDS4 raster examples
--------------------

Listing bands and subdatasets:

::

   $ gdalinfo b0011_p237201_01_01v02.xml

   Driver: PDS4/NASA Planetary Data System 4
   Files: b0011_p237201_01_01v02.xml
          b0011_p237201_01_01v02.qub
   Size is 512, 512
   Coordinate System is `'
   Image Structure Metadata:
     INTERLEAVE=BAND
   Subdatasets:
     SUBDATASET_1_NAME=PDS4:b0011_p237201_01_01v02.xml:1:1
     SUBDATASET_1_DESC=Image file b0011_p237201_01_01v02.qub, array Spectral_Qube_Object
     SUBDATASET_2_NAME=PDS4:b0011_p237201_01_01v02.xml:1:2
     SUBDATASET_2_DESC=Image file b0011_p237201_01_01v02.qub, array iof_r2
     SUBDATASET_3_NAME=PDS4:b0011_p237201_01_01v02.xml:1:3
     SUBDATASET_3_DESC=Image file b0011_p237201_01_01v02.qub, array iof_r7
     SUBDATASET_4_NAME=PDS4:b0011_p237201_01_01v02.xml:1:4
   [...]
     SUBDATASET_16_DESC=Image file b0011_p237201_01_01v02.qub, array emission_angle
     SUBDATASET_17_NAME=PDS4:b0011_p237201_01_01v02.xml:1:17
     SUBDATASET_17_DESC=Image file b0011_p237201_01_01v02.qub, array phase_angle
     SUBDATASET_18_NAME=PDS4:b0011_p237201_01_01v02.xml:1:18
     SUBDATASET_18_DESC=Image file b0011_p237201_01_01v02.qub, array approx_incidence_angle
     SUBDATASET_19_NAME=PDS4:b0011_p237201_01_01v02.xml:1:19
     SUBDATASET_19_DESC=Image file b0011_p237201_01_01v02.qub, array approx_emission_angle
     SUBDATASET_20_NAME=PDS4:b0011_p237201_01_01v02.xml:1:20
     SUBDATASET_20_DESC=Image file b0011_p237201_01_01v02.qub, array approx_phase_angle
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,  512.0)
   Upper Right (  512.0,    0.0)
   Lower Right (  512.0,  512.0)
   Center      (  256.0,  256.0)
   Band 1 Block=512x1 Type=Int16, ColorInterp=Undefined
     Offset: 0.146998785514825,   Scale:4.48823844390647e-06
   Band 2 Block=512x1 Type=Int16, ColorInterp=Undefined
     Offset: 0.146998785514825,   Scale:4.48823844390647e-06
   Band 3 Block=512x1 Type=Int16, ColorInterp=Undefined
     Offset: 0.146998785514825,   Scale:4.48823844390647e-06
   Band 4 Block=512x1 Type=Int16, ColorInterp=Undefined
     Offset: 0.146998785514825,   Scale:4.48823844390647e-06
   Band 5 Block=512x1 Type=Int16, ColorInterp=Undefined
     Offset: 0.146998785514825,   Scale:4.48823844390647e-06

The information displayed by default is the one of the first subdataset
(SUBDATASET_1_NAME)

Getting information on a subdataset:

::

   $ gdalinfo PDS4:b0011_p237201_01_01v02.xml:1:2

   Driver: PDS4/NASA Planetary Data System 4
   Files: b0011_p237201_01_01v02.xml
          b0011_p237201_01_01v02.qub
   Size is 512, 512
   Coordinate System is `'
   Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,  512.0)
   Upper Right (  512.0,    0.0)
   Lower Right (  512.0,  512.0)
   Center      (  256.0,  256.0)
   Band 1 Block=512x1 Type=Int16, ColorInterp=Undefined
     Offset: 0.04984971,   Scale:7.454028e-06

Conversion to GeoTIFF of a given subdatasets:

::

   $ gdal_translate PDS4:b0011_p237201_01_01v02.xml:1:2 iof_r2.tif

Conversion to GeoTIFF of a all subdatasets:

::

   $ gdal_translate -sds b0011_p237201_01_01v02.xml b0011_p237201_01_01v02.tif

This will create b0011_p237201_01_01v02_X.tif files where X=1,....,N

Creation of a new PDS4 dataset, using the default template and setting
its parameterized variables:

::

   $ gdal_translate input.tif output.xml -of PDS4 \
               -co VAR_TARGET_TYPE=Satellite \
               -co VAR_Target=Moon \
               -co VAR_OBSERVING_SYSTEM_NAME=LOLA \
               -co VAR_LOGICAL_IDENTIFIER=Lunar_LRO_LOLA_DEM_Global_64ppd.tif \
               -co VAR_TITLE="LRO LOLA Digital Elevation Model (DEM) 64ppd" \
               -co VAR_INVESTIGATION_AREA_NAME="Lunar Reconnaissance Orbiter" \
               -co VAR_INVESTIGATION_AREA_LID_REFERENCE="urn:nasa:pds:context:instrument_host:spacecraft.lro"

Creation of the same PDS4 dataset as above, using the default template
but setting its parameterized variables from a text file. Helps with
long command lines:

Create a text file "myOptions.txt" with the below content

::

   #This is a comment
   #Conversion parameters for the LRO LOLA dataset
   -co VAR_TARGET_TYPE=Satellite
   -co VAR_Target=Moon
   -co VAR_OBSERVING_SYSTEM_NAME=LOLA
   -co VAR_LOGICAL_IDENTIFIER=Lunar_LRO_LOLA_DEM_Global_64ppd.tif
   -co VAR_TITLE="LRO LOLA Digital Elevation Model (DEM) 64ppd"
   -co VAR_INVESTIGATION_AREA_NAME="Lunar Reconnaissance Orbiter"
   -co VAR_INVESTIGATION_AREA_LID_REFERENCE="urn:nasa:pds:context:instrument_host:spacecraft.lro"
   #end of file

::

   gdal_translate input.tif output.xml -of PDS4 --optfile myOptions.txt

For more on --optfile, consult `the general documentation on GDAL
utilities <gdal_utilities.html>`__.

Creation of a PDS4 dataset, using a non default template (here on a HTTP
server, but local filename also possible):

::

   $ gdal_translate input.tif output.xml -of PDS4 \
               -co TEMPLATE=http://example.com/mytemplate.xml

Creation of a PDS4 dataset from a source PDS4 dataset (using the XML
file of this source PDS4 dataset as an implicit template), with
subsetting:

::

   $ gdal_translate input.xml output.xml -of PDS4 -projwin ullx ully lrx lry

In Python, creation of a PDS4 dataset from a GeoTIFF, using a base
template into which one substitute one element with a new value:

::

   from osgeo import gdal
   from lxml import etree

   # Customization of template
   template = open('template.xml','rb').read()
   root = etree.XML(template)
   ns = '{http://pds.nasa.gov/pds4/pds/v1}'
   identifier = root.find(".//{ns}Identification_Area/{ns}logical_identifier".format(ns = ns))
   identifier.text = 'new_identifier'

   # Serialize the modified template in a in-memory file
   in_memory_template = '/vsimem/template.xml'
   gdal.FileFromMemBuffer(in_memory_template, etree.tostring(root))

   # Create the output dataset
   gdal.Translate('out.xml', 'in.tif', format = 'PDS4',
                  creationOptions = ['TEMPLATE='+in_memory_template])

   # Cleanup
   gdal.Unlink(in_memory_template)

Appending a new image (subdataset) to an existing PDS4 dataset.

::

   $ gdal_translate new_image.tif existing_output.xml -of PDS4 \
                         -co APPEND_SUBDATASET=YES \
                         -co ARRAY_IDENTIFIER=my_new_image


Adding a PDS4 label to an existing ISIS3 dataset. (GDAL >= 3.1)

::

   $ gdal_translate dataset.cub dataset.xml -of PDS4 -co CREATE_LABEL_ONLY=YES

PDS4 vector examples
--------------------

Displaying the content of a PDS4 dataset with a table:

::

   $ ogrinfo -al my_pds4.xml

Converting a PDS4 dataset with a table to shapefile, by specifying
columns that contain longitude and latitude:

::

   $ ogr2ogr out.shp my_pds4.xml -oo LAT=my_lat_column -oo LONG=my_long_column

Converting a shapefile to a PDS4 dataset with a CSV-delimited table
(with an implicit WKT column to store the geometry):

::

   $ ogr2ogr my_out_pds4.xml in.shp

Limitations
-----------

As a new driver and new format, please report any issues to the bug
tracker, as explained on the `wiki <https://trac.osgeo.org/gdal/wiki>`__

See Also:
---------

-  Implemented as ``gdal/frmts/pds/pds4dataset.cpp``.
-  `Official
   documentation <https://pds.nasa.gov/pds4/doc/index.shtml>`__
-  `Schemas, including the cartography
   extension <https://pds.nasa.gov/pds4/schema/released/>`__
-  :ref:`raster.pds` driver.
-  :ref:`raster.isis3` driver.
