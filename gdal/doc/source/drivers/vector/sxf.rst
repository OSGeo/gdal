.. _vector.sxf:

Storage and eXchange Format - SXF
=================================

.. shortname:: SXF

.. built_in_by_default::

This driver reads SXF files, open format often associated with Russian
GIS Software Panorama.

The driver is read only, but supports deletion of data source. The
driver supports SXF binary files version 3.0 and higher.

The SXF layer support the following capabilities:

-  Strings as UTF8
-  Random Read
-  Fast Feature Count
-  Fast Get Extent
-  Fast Set Next By Index

The driver uses classifiers (RSC files) to map feature from SXF to
layers. Features that do not belong to any layer are put to the layer
named "Not_Classified". The layers with zero features are not present in
data source.

To be used automatically, the RSC file should have the same name as SXF
file. User can provide RSC file path using config option
**SXF_RSC_FILENAME**. This config option overrides the use of same name
RSC.

The RSC file usually stores long and short layer name. The long name is
usually in Russian, and short in English. The **SXF_LAYER_FULLNAME**
config option allows choosing which layer names to use. If
SXF_LAYER_FULLNAME is TRUE - the driver uses long names, if FALSE -
short.

The attributes are read from SXF file. Maximum number of fields is
created for the same layer features with different number of attributes.
If attribute has a code mapped to RSC file, driver adds only the code
(don't get real value from RSC, as the value type may differ from field
type).

If config option **SXF_SET_VERTCS** set to ON, the layers spatial
reference will include vertical coordinate system description if exist.

Since GDAL 3.1 config options can be passed as driver open options.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

See Also
--------

-  `Panorama web page <http://gisinfo.ru>`__
-  `SXF binary format description v.4 (rus) -
   pdf <http://gistoolkit.ru/download/doc/sxf4bin.pdf>`__
-  `SXF binary format description v.4 (rus) -
   doc <http://gistoolkit.ru/download/classifiers/formatsxf.zip>`__
-  `SXF format description v.3
   (rus) <http://loi.sscc.ru/gis/formats/Format-geo/sxf/sxf3-231.txt>`__
-  `RSC format description
   (rus) <http://gistoolkit.ru/download/classifiers/formatrsc.zip>`__
-  `Test spatial data in SXF format
   (rus) <http://www.gisinfo.ru/price/price_map.htm>`__
-  `Some RSC files
   (rus) <http://www.gisinfo.ru/classifiers/classifiers.htm>`__
