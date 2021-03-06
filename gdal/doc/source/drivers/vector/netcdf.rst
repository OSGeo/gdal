.. _vector.netcdf:

NetCDF: Network Common Data Form - Vector
=========================================

.. versionadded:: 2.1

.. shortname:: netCDF

.. build_dependencies:: libnetcdf

The netCDF driver supports read and write
(creation from scratch and in some cases append operations) to vector datasets (you
can find documentation for the :ref:`raster side <raster.netcdf>`)

NetCDF is an interface for array-oriented data access and is used for
representing scientific data.

The driver handles the "point" and "profile" `feature
types <http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html#_features_and_feature_types>`__
of the CF 1.6 convention. For CF-1.7 and below (as well as non-CF files), it also supports a more custom approach for
non-point geometries.

The driver also supports writing and reading from CF-1.8 convention compliant files that
have simple geometry information encoded within them.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Conventions and Data Formats
----------------------------
The netCDF vector driver supports reading and writing netCDF files following the Climate and Forecast (CF) Metadata Conventions.
Vector datasets can be written using the simple geometry specification of the CF-1.8 convention, or by using the CF-1.6 convention
and by writing non-point geometry items as WKT.

Distinguishing the Two Formats
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Upon reading a netCDF file, the driver will attempt to read the global *Conventions* attribute. If it's value is *CF-1.8* or higher (in this exact
format, as specified in the CF convention) then the driver will treat the netCDF file as one that has *CF-1.8* geometries contained within
it. If the *Conventions* attribute has a value of CF-1.6, the the file will be treated as following the CF-1.6 convention.

CF-1.8 Writing Limitations
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Writing to a CF-1.8 netCDF dataset poses some limitations. Only writing the feature types specified by the CF-1.8 standard (see
section `Geometry <#geometry>`__ for more details) are supported, and measured features are only partially supported.
Other geometries, such as non-simple curve geometries, are not supported in any way.

CF-1.8 datasets also do not support the *append* access mode.

There are what are considered *reserved variable names* for CF-1.8 datasets. These variable names are used by the driver to store its metadata.
Refrain from using these names as layer names to avoid naming conflicts when writing datasets with multiple layers.

Suppose a layer in a CF-1.8 dataset has the name LAYER with a field with name FIELD. Then the following names would be considered *reserved*:

-  *LAYER_node_coordinates*: used to store point information
-  *LAYER_node_count*: used to store per shape point count information (not created if LAYER has a geometry type of Point)
-  *LAYER_part_node_count*: used to store per part point count information (only created if LAYER consists of MultiLineStrings, MultiPolygons, or has at least one Polygon with interior rings)
-  *LAYER_interior_ring*: used to store interior ring information (only created if LAYER consists of at least one Polygon with interior rings)
-  *LAYER_field_FIELD*: used to store field information for FIELD.

These names are the only reserved names applying to CF-1.8 datasets.

CF-1.6/WKT datasets are not limited to the aforementioned restrictions.

Mapping of concepts
-------------------

Field types
~~~~~~~~~~~

On creation of netCDF files, the mapping between OGR field types and
netCDF type is the following :

================ ==================================================
OGR field type   netCDF type
================ ==================================================
String(1)        char
String           char (bi-dimensional), or string for NC4
Integer          int
Integer(Boolean) byte
Integer(Int16)   short
Integer64        int64 for NC4, or double for NC3 as a fallback
Real             double
Real(Float32)    float
Date             int (with units="days since 1970-1-1")
DateTime         double (with units="seconds since 1970-1-1 0:0:0")
================ ==================================================

The driver also writes the following attributes for each OGR fields /
netCDF variables.

-  *ogr_field_name*: OGR field name (useful if the netCDF variable name
   is different, due to collision)
-  *ogr_field_type*: OGR field type (such as
   String,Integer,Date,DateTime,etc...)
-  *ogr_field_width*: OGR field width. Only set if it is non-zero,
   except for strings
-  *ogr_field_precision*: OGR field precision. Only set if it is
   non-zero

They are written by default (unless the
`WRITE_GDAL_TAGS <#WRITE_GDAL_TAGS>`__ dataset creation option is set to
NO). They are not required for reading, but may help to better identify
field characteristics

On reading, the mapping is the following :

================================================== ==============
netCDF type                                        OGR field type
================================================== ==============
byte                                               Integer
ubyte (NC4 only)                                   Integer
char (mono dimensional)                            String(1)
char (bi dimensional)                              String
string (NC4 only)                                  String
short                                              Integer(Int16)
ushort (NC4 only)                                  Integer
int                                                Integer
int or double (with units="days since 1970-1-1")   Date
uint (NC4 only)                                    Integer64
int64 (NC4 only)                                   Integer64
uint64 (NC4 only)                                  Real
float                                              Real(Float32)
double                                             Real
double (with units="seconds since 1970-1-1 0:0:0") DateTime
================================================== ==============

Layers
~~~~~~
In the CF-1.8 compliant driver, a single layer corresponds to a single
**geometry container** within a CF-1.8 compliant netCDF file. A geometry container, per
the CF-1.8 specification, is referred to by another variable
(presumably a data variable) through the **geometry** attribute. When reading
a CF-1.8 compliant netCDF file, all geometry containers within the netCDF file
will be present in the opened dataset as separate layers. Similarly, when writing to
a CF-1.8 dataset, each layer will be written to a geometry container whose variable
name is that of the source layer. When writing to a CF-1.8 dataset specifically, multiple layers are always
enabled and are always in a single netCDF file, regardless of the `MULTIPLE_LAYERS <#MULTIPLE_LAYERS>`__ option.

When working with files made with older versions of the driver (pre CF-1.8),
a single netCDF file generally corresponds to a single OGR layer,
provided that it contains only mono-dimensional variables,
indexed by the same dimension (or bi-dimensional variables of type char).
For netCDF v4 files with multiple groups, each group may be seen as a separate OGR
layer. On writing, the `MULTIPLE_LAYERS <#MULTIPLE_LAYERS>`__ dataset creation
option can be used to control whether multiple layers is disabled, or if
multiple layers should go in separate files, or separate groups.

Strings
~~~~~~~

Variable length strings are not natively supported in netCDF v3 format.
To work around that, OGR uses bi-dimensional char variables, whose first
dimension is the record dimension, and second dimension the maximum
width of the string.

By default, OGR implements a "auto-grow" mode in
writing, where the maximum width of the variable used to store a OGR
string field is extended when needed.

For WKT datasets, this leads to a full
rewrite of already written records; although this process is transparent for the user,
it can slow down the creation process in non-linear ways. A similar
mechanism is used to handle layers with geometry types other than point
to store the ISO WKT representation of the geometries.

For CF-1.8 datasets, growing the string width dimension is
a relatively inexpensive process which does not involve recopying of records, but involves
only a simple integer reassignment. Because of how inexpensive dimension growth is with CF-1.8 datasets,
auto growth of the string width dimension is always on.

When using a netCDF v4 output format (NC4), strings will be by default
written as netCDF v4 variable length strings.

Geometry
~~~~~~~~

Supported feature types when reading from a CF-1.8 convention compliant netCDF file
include OGRPoint, OGRLineString, OGRPolygon, OGRMultiPoint, OGRMultiLineString, and
OGRMultiPolygon. Due to slight ambiguities present in the CF-1.8 convention concerning
Polygons versus MultiPolygons, the driver will in most cases default to assuming a MultiPolygon
for the geometry of a layer with **geometry_type** polygon. The one exception where a Polygon type
will be used is when the attribute **part_node_count** is not present within that layer's geometry container.
Per convention requirements, the driver supports reading and writing from geometries with X, Y, and Z axes.
Writing from source layers with features containing an M axis is also partially supported. The X, Y, and Z
information of a measured feature will be able to be captured in a CF-1.8 netCDF file, but the measure information
will be lost completely.

When working with a CF-1.6/WKT dataset, layers with a geometry type
of Point or Point25D will cause the implicit creation of x,y(,z)
variables for a projected coordinate system, or lon,lat(,z) variables
for geographic coordinate systems. For other
geometry types, a variable "ogc_wkt" ( bi-dimensional char for NC3
output, or string for NC4 output) is created and used to store the
geometry as a ISO WKT string.

"Profile" feature type
~~~~~~~~~~~~~~~~~~~~~~

The driver can handle "profile" feature type, i.e. phenomenons that
happen at a few positions along a vertical line at a fixed horizontal
position. In that representation, some variables are indexed by the
profile, and others by the observation.

More precisely, the driver supports reading and writing profiles
organized accordingly with the "`Indexed ragged array
representation <http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html#_indexed_ragged_array_representation_of_profiles>`__"
of profiles.

On reading, the driver will collect values of variables indexed by the
profile dimension and expose them as long as variables indexed by the
observation dimension, based on a variable such as "parentIndex" with an
attribute "instance_dimension" pointing to the profile dimension.

On writing, the `FEATURE_TYPE <#FEATURE_TYPE>`__\ =PROFILE layer
creation option must be set and the driver will need to be instructed
which OGR fields are indexed either by the profile or by the observation
dimension. The list of fields indexed by the profile can be specified
with the `PROFILE_VARIABLES <#PROFILE_VARIABLES>`__ layer creation
options (other fields are assumed to be indexed by the observation
dimension). Fields indexed by the profile are the horizontal geolocation
(created implicitly), and other user attributes such as the location
name, etc. Care should be taken into selecting which variables are
indexed by the profile dimension: given 2 OGR features (taking into
account only the variables indexed by the profile dimension), if they
have different values for such variables, they will be considered to
belong to different profiles.

In the below example, the station_name and time variables may be indexed
by the profile dimension (the geometry is assumed to be also indexed by
the profile dimension), since all records that have the same value for
one of those variables have same values for the other ones, whereas
temperature and Z should be indexed by the default dimension.

============ ==================== ================== =========== ===
station_name time                 geometry           temperature Z
============ ==================== ================== =========== ===
Paris        2016-03-01T00:00:00Z POINT (2 49)       25          100
Vancouver    2016-04-01T12:00:00Z POINT (-123 49.25) 5           100
Paris        2016-03-01T00:00:00Z POINT (2 49)       3           500
Vancouver    2016-04-01T12:00:00Z POINT (-123 49.25) -15         500
============ ==================== ================== =========== ===

An integer field, with the name of the profile dimension (whose default
name is "profile", which can be altered with the
`PROFILE_DIM_NAME <#PROFILE_DIM_NAME>`__ layer creation option), will be
used to store the automatically computed id of profile sites (unless a
integer OGR field with the same name exits).

The size of the profile dimension defaults to 100 for non-NC4 output
format, and is extended automatically in case of additional profiles
(with similar performance issues as growing strings). For NC4 output
format, the profile dimension is of unlimited size by default.

Dataset creation options
------------------------

-  **GEOMETRY_ENCODING**\ =CF_1.8/WKT: Chooses which geometry encoding to use
   when creating new layers within the dataset. Default is CF_1.8.
-  **FORMAT**\ =NC/NC2/NC4/NC4C: netCDF format. NC is the classic netCDF
   format (compatible of netCDF v3.X and 4.X libraries). NC2 is the
   extension of NC for files larger than 4 GB. NC4 is the netCDF v4
   format, using a HDF5 container, offering new capabilities (new types,
   concept of groups, etc...) only available in netCDF v4 library. NC4C
   is a restriction of the NC4 format to the concepts supported by the
   classic netCDF format. Default is NC.
-  **WRITE_GDAL_TAGS**\ =YES/NO: Whether to write GDAL specific
   information as netCDF attributes. Default is YES.
-  **CONFIG_FILE**\ =string. Path to a `XML configuration
   file <#xml-configuration-file>`__ (or its content inlined) for precise control of
   the output.

The following option will only have effect when simultaneously specifying GEOMETRY_ENCODING=WKT:

-  **MULTIPLE_LAYERS**\ =NO/SEPARATE_FILES/SEPARATE_GROUPS. Default is
   NO, i.e a dataset can contain only a single OGR layer. SEPARATE_FILES
   can be used to put the content of each OGR layer in a single netCDF
   file, in which case the name passed at dataset creation is used as
   the directory, and the layer name is used as the basename of the
   netCDF file. SEPARATE_GROUPS may be used when FORMAT=NC4 to put each
   OGR layer in a separate netCDF group, inside the same file.

Layer creation options
----------------------

The following option applies to both dataset types:

-  **USE_STRING_IN_NC4**\ =YES/NO. Whether to use NetCDF string type for
   strings in NC4 format. If NO, bidimensional char variable are used.
   Default to YES when FORMAT=NC4.

The following options require a dataset with GEOMETRY_ENCODING=WKT:

-  **RECORD_DIM_NAME**\ =string. Name of the unlimited dimension that
   index features. Defaults to "record".
-  **STRING_DEFAULT_WIDTH**\ =int. Default width of strings (when using
   bi-dimensional char variables). Default is 10 in autogrow mode, 80
   otherwise.
-  **WKT_DEFAULT_WIDTH**\ =int. Default width of WKT strings (when using
   bi-dimensional char variables). Default is 1000 in autogrow mode,
   10000 otherwise.
-  **AUTOGROW_STRINGS**\ =YES/NO. Whether to auto-grow string fields of
   non-fixed width, or ogc_wkt special field, when serialized as
   bidimensional char variables. Default is YES. When set to NO, if the
   string is larger than its maximum initial width (set by
   STRING_DEFAULT_WIDTH), it is truncated. For a geometry, it is
   completely discarded.
-  **FEATURE_TYPE**\ =AUTO/POINT/PROFILE. Select the CF FeatureType.
   Defaults to AUTO where FeatureType=Point is selected if the layer
   geometry type is Point, otherwise the custom approach involving the
   "ogc_wkt" field is used. Can be set to `PROFILE <#profile>`__ so as
   to select the creation of an indexed ragged array representation of
   profiles.
-  **PROFILE_DIM_NAME**\ =string. Name of the profile dimension and
   variable. Defaults to "profile". Only used when FEATURE_TYPE=PROFILE.
-  **PROFILE_DIM_INIT_SIZE**\ =int or string. Initial size of profile
   dimension, or UNLIMITED for NC4 files. Defaults to 100 when FORMAT !=
   NC4 and to UNLIMITED when FORMAT = NC4. Only used when
   FEATURE_TYPE=PROFILE.
-  **PROFILE_VARIABLES**\ =string. Comma separated list of field names
   that must be indexed by the profile dimension. Only used when
   FEATURE_TYPE=PROFILE.

The following option requires a dataset with GEOMETRY_ENCODING=CF_1.8:

-  **BUFFER_SIZE**\ =int. The soft limit of the write buffer in bytes. Larger
   values generally imply better performance, but values should be comfortably
   less than that of available physical memory or else thrashing can occur.
   By default, this value is set at 20% of usable physical memory (usable meaning
   total physical RAM considering limitations of virtual address space size).
   Buffer contents are committed between translating features, but not *during*
   translating a feature, so this limit does not apply to a single feature. The minimum
   acceptable size is 4096. If a value lower than this is specified the default will
   be used.
-  **GROUPLESS_WRITE_BACK**\ =YES/NO. In order to reduce time used to write data to the target
   netCDF file, data is often grouped together in arrays and written all at once.
   Each of these arrays is associated with a variable in the target dataset. 
   Arrays are destroyed as soon as the associated data is written to the netCDF file
   which in turn occurs as soon as a complete data array for a variable is assembled in memory.
   For machines with small memory sizes, this optimization may cause issues
   when writing large datasets with large layers. Turning this option on by specifying "YES" disables array writing
   and causes data to be written one datum at a time. It is strongly recommended to keep this option off
   unless out of memory errors or performance issues occur. In the general case,
   this technique greatly improves translation efficiency. The default value is NO.

XML configuration file
----------------------

A XML configuration file conforming to the following
`schema <https://github.com/OSGeo/gdal/blob/master/gdal/data/netcdf_config.xsd>`__
can be used for very precise control on the output format, in particular
to set all needed attributes (such as units) to conform to the `NetCDF
CF-1.6
convention <http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html>`__.

It has been designed in particular, but not exclusively, to be usable in
use cases involving the `MapServer OGR
output <http://mapserver.org/output/ogr_output.html>`__.

Such a file can be used to :

-  set dataset and layer creation options.
-  set global netCDF attributes.
-  map OGR field names to netCDF variable names.
-  set netCDF attributes attached to netCDF variables.

The scope of effect is either globally, when elements are defined as
direct children of the root <Configuration> node, or specifically to a
given layer, when defined as children of a <Layer> node.

The filename is specified with the CONFIG_FILE dataset creation option.
Alternatively, the content of the file can be specified inline as the
value of the option (it must then begin strictly with the
"<Configuration" characters)

The following example shows all possibilities and precedence rules:

::

   <Configuration>
       <DatasetCreationOption name="FORMAT" value="NC4"/>
       <DatasetCreationOption name="MULTIPLE_LAYERS" value="SEPARATE_GROUPS"/>
       <LayerCreationOption name="RECORD_DIM_NAME" value="observation"/>
   <!-- applies to all layers -->
       <Attribute name="copyright" value="Copyright(C) 2016 Example"/>
       <Field name="weight">  <!-- edit user field/variable -->
           <Attribute name="units" value="kg"/>
           <Attribute name="maximum" value="10" type="double"/>
       </Field>
       <Field netcdf_name="z"> <!-- edit predefined variable -->
           <Attribute name="long_name" value="Elevation"/>
       </Field>
   <!-- start of layer specific definitions -->
       <Layer name="1st_layer" netcdf_name="firstlayer"> <!-- OGR layer "1st_layer" is renamed as "firstlayer" netCDF group -->
           <LayerCreationOption name="FEATURE_TYPE" value="POINT"/>
           <Attribute name="copyright" value="Public domain"/> <!-- override global one -->
           <Attribute name="description" value="This is my first layer"/> <!-- additional attribute -->
           <Field name="1st_field" netcdf_name="firstfield"/> <!-- rename OGR field "1st_field" as the "firstfield" netCDF variable -->
           <Field name="weight"/> <!-- cancel above global customization -->
           <Field netcdf_name="lat"> <!-- edit predefined variable -->
               <Attribute name="long_name" value=""/> <!-- remove predefined attribute -->
           </Field>
       </Layer>
       <Layer name="sounding">
           <LayerCreationOption name="FEATURE_TYPE" value="PROFILE"/>
           <Field name="station_name" main_dim="profile"/> <!-- the corresponding netCDF variable will be indexed against the profile dimension, instead of the observation dimension -->
           <Field name="time" main_dim="profile"/> <!-- the corresponding netCDF variable will be indexed against the profile dimension, instead of the observation dimension -->
       </Layer>
   </Configuration>

The effect on the output can be checked by running the **ncdump**
utility

Further Reading
---------------

-  :ref:`Raster side of the netCDF driver. <raster.netcdf>`
-  `NetCDF CF-1.6
   convention <http://cfconventions.org/cf-conventions/v1.6.0/cf-conventions.html>`__
-  `NetCDF CF-1.8
   convention draft <https://github.com/cf-convention/cf-conventions/blob/master/ch07.adoc>`__
-  `NetCDF compiled
   libraries <http://www.unidata.ucar.edu/downloads/netcdf/index.jsp>`__
-  `NetCDF
   Documentation <http://www.unidata.ucar.edu/software/netcdf/docs/>`__

Credits
-------

Development of the read/write vector capabilities for netCDF was funded
by `Meteorological Service of
Canada <https://www.ec.gc.ca/meteo-weather/>`__ , `World Ozone and
Ultraviolet Radiation Data Centre <http://woudc.org>`__, and the `US Geological Survey <https://www.usgs.gov>`__.
