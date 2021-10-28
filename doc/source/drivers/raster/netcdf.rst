.. _raster.netcdf:

================================================================================
NetCDF: Network Common Data Form
================================================================================

.. shortname:: netCDF

.. build_dependencies:: libnetcdf

This format is supported for read and write access. This page only
describes the raster support (you can find documentation for the :ref:`vector
side <vector.netcdf>`) NetCDF is an interface for
array-oriented data access and is used for representing scientific data.

The fill value metadata or missing_value backward compatibility is
preserved as NODATA value when available.

NOTE: Implemented as ``gdal/frmts/netcdf/netcdfdataset.cpp``.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_create::

.. supports_georeferencing::

Multiple Image Handling (Subdatasets)
-------------------------------------

Network Command Data Form is a container for several different arrays
most used for storing scientific dataset. One NetCDF file may contain
several datasets. They may differ in size, number of dimensions and may
represent data for different regions.

If the file contains only one NetCDF array which appears to be an image,
it may be accessed directly, but if the file contains multiple images it
may be necessary to import the file via a two step process.

The first step is to get a report of the components images (dataset) in
the file using gdalinfo, and then to import the desired images using
gdal_translate. The gdalinfo utility lists all multidimensional
subdatasets from the input NetCDF file.

The name of individual images are assigned to the SUBDATASET_n_NAME
metadata item. The description for each image is found in the
SUBDATASET_n_DESC metadata item. For NetCDF images will follow this
format: *NETCDF:filename:variable_name*

where *filename* is the name of the input file, and *variable_name* is
the dataset selected within the file.

On the second step you provide this name for **gdalinfo** to get
information about the dataset or **gdal_translate** to read dataset.

For example, we want to read data from a NetCDF file:

::

   $ gdalinfo sst.nc
   Driver: netCDF/Network Common Data Format
   Size is 512, 512
   Coordinate System is `'
   Metadata:
     NC_GLOBAL#title=IPSL  model output prepared for IPCC Fourth Assessment SRES A2 experiment
     NC_GLOBAL#institution=IPSL (Institut Pierre Simon Laplace, Paris, France)
     NC_GLOBAL#source=IPSL-CM4_v1 (2003) : atmosphere : LMDZ (IPSL-CM4_IPCC, 96x71x19) ; ocean ORCA2 (ipsl_cm4_v1_8, 2x2L31); sea ice LIM (ipsl_cm4_v
     NC_GLOBAL#contact=Sebastien Denvil, sebastien.denvil@ipsl.jussieu.fr
     NC_GLOBAL#project_id=IPCC Fourth Assessment
     NC_GLOBAL#table_id=Table O1 (13 November 2004)
     NC_GLOBAL#experiment_id=SRES A2 experiment
     NC_GLOBAL#realization=1
     NC_GLOBAL#cmor_version=9.600000e-01
     NC_GLOBAL#Conventions=CF-1.0
     NC_GLOBAL#history=YYYY/MM/JJ: data generated; YYYY/MM/JJ+1 data transformed  At 16:37:23 on 01/11/2005, CMOR rewrote data to comply with CF standards and IPCC Fourth Assessment requirements
     NC_GLOBAL#references=Dufresne et al, Journal of Climate, 2015, vol XX, p 136
     NC_GLOBAL#comment=Test drive
   Subdatasets:
     SUBDATASET_1_NAME=NETCDF:"sst.nc":lon_bnds
     SUBDATASET_1_DESC=[180x2] lon_bnds (64-bit floating-point)
     SUBDATASET_2_NAME=NETCDF:"sst.nc":lat_bnds
     SUBDATASET_2_DESC=[170x2] lat_bnds (64-bit floating-point)
     SUBDATASET_3_NAME=NETCDF:"sst.nc":time_bnds
     SUBDATASET_3_DESC=[24x2] time_bnds (64-bit floating-point)
     SUBDATASET_4_NAME=NETCDF:"sst.nc":tos
     SUBDATASET_4_DESC=[24x170x180] sea_surface_temperature (32-bit floating-point)Corner Coordinates:
   Upper Left  (    0.0,    0.0)
   Lower Left  (    0.0,  512.0)
   Upper Right (  512.0,    0.0)
   Lower Right (  512.0,  512.0)
   Center      (  256.0,  256.0)

This NetCDF files contain 4 datasets, lon_bnds, lat_bnds, tim_bnds and
tos. Now select the subdataset, described as: ``NETCDF:"sst.nc":tos``
``[24x17x180] sea_surface_temperature (32-bit floating-point)``
and get the information about the number of bands there is inside this
variable.

::

   $ gdalinfo NETCDF:"sst.nc":tos
   Driver: netCDF/Network Common Data Format
   Size is 180, 170
   Coordinate System is `'
   Origin = (1.000000,-79.500000)
   Pixel Size = (1.98888889,0.99411765)
   Metadata:
     NC_GLOBAL#title=IPSL  model output prepared for IPCC Fourth Assessment SRES A2 experiment
     NC_GLOBAL#institution=IPSL (Institut Pierre Simon Laplace, Paris, France)

.... More metadata

::

     time#standard_name=time
     time#long_name=time
     time#units=days since 2001-1-1
     time#axis=T
     time#calendar=360_day
     time#bounds=time_bnds
     time#original_units=seconds since 2001-1-1
   Corner Coordinates:
   Upper Left  (   1.0000000, -79.5000000)
   Lower Left  (   1.0000000,  89.5000000)
   Upper Right (     359.000,     -79.500)
   Lower Right (     359.000,      89.500)
   Center      ( 180.0000000,   5.0000000)
   Band 1 Block=180x1 Type=Float32, ColorInterp=Undefined
     NoData Value=1e+20
     Metadata:
       NETCDF_VARNAME=tos
       NETCDF_DIMENSION_time=15
       NETCDF_time_units=days since 2001-1-1
   Band 2 Block=180x1 Type=Float32, ColorInterp=Undefined
     NoData Value=1e+20
     Metadata:
       NETCDF_VARNAME=tos
       NETCDF_DIMENSION_time=45
       NETCDF_time_units=days since 2001-1-1

.... More Bands

::

   Band 22 Block=180x1 Type=Float32, ColorInterp=Undefined
     NoData Value=1e+20
     Metadata:
       NETCDF_VARNAME=tos
       NETCDF_DIMENSION_time=645
       NETCDF_time_units=days since 2001-1-1
   Band 23 Block=180x1 Type=Float32, ColorInterp=Undefined
     NoData Value=1e+20
     Metadata:
       NETCDF_VARNAME=tos
       NETCDF_DIMENSION_time=675
       NETCDF_time_units=days since 2001-1-1
   Band 24 Block=180x1 Type=Float32, ColorInterp=Undefined
     NoData Value=1e+20
     Metadata:
       NETCDF_VARNAME=tos
       NETCDF_DIMENSION_time=705
       NETCDF_time_units=days since 2001-1-1

gdalinfo displays the number of bands into this subdataset. There are
metadata attached to each band. In this example, the metadata informs us
that each band correspond to an array of monthly sea surface temperature
from January 2001. There are 24 months of data in this subdataset. You
may also use **gdal_translate** for reading the subdataset.

Note that you should provide exactly the contents of the line marked
**SUBDATASET_n_NAME** to GDAL, including the **NETCDF:** prefix.

The **NETCDF** prefix must be first. It triggers the subdataset NetCDF
driver. This driver is intended only for importing remote sensing and
geospatial datasets in form of raster images. If you want explore all
data contained in NetCDF file you should use another tools.

Dimension
---------

The NetCDF driver assume that data follows the CF-1 convention from
`UNIDATA <http://www.unidata.ucar.edu/software/netcdf/docs/conventions.html>`__
The dimensions inside the NetCDF file use the following rules: (Z,Y,X).
If there are more than 3 dimensions, the driver will merge them into
bands. For example if you have an 4 dimension arrays of the type (P, T,
Y, X). The driver will multiply the last 2 dimensions (P*T). The driver
will display the bands in the following order. It will first increment T
and then P. Metadata will be displayed on each band with its
corresponding T and P values.

Georeference
------------

There is no universal way of storing georeferencing in NetCDF files. The
driver first tries to follow the CF-1 Convention from UNIDATA looking
for the Metadata named "grid_mapping". If "grid_mapping" is not present,
the driver will try to find an lat/lon grid array to set geotransform
array. The NetCDF driver verifies that the Lat/Lon array is equally
spaced.

.. versionadded:: 3.4 crs_wkt attribute support

If those 2 methods fail, NetCDF driver will try to read the following
metadata directly and set up georeferencing.

-  spatial_ref (Well Known Text)

-  GeoTransform (GeoTransform array)

or,

-  Northernmost_Northing
-  Southernmost_Northing
-  Easternmost_Easting
-  Westernmost_Easting

See also the configuration options **GDAL_NETCDF_VERIFY_DIMS** and
**GDAL_NETCDF_IGNORE_XY_AXIS_NAME_CHECKS** which control this
behavior.

Open options
------------

The following open options are available:

-  **HONOUR_VALID_RANGE**\ =YES/NO: (GDAL > 2.2) Whether to set to
   nodata pixel values outside of the validity range indicated by
   valid_min, valid_max or valid_range attributes. Default is YES.

Creation Issues
---------------

This driver supports creation of NetCDF file following the CF-1
convention. You may create set of 2D datasets. Each variable array is
named Band1, Band2, ... BandN.

Each band will have metadata tied to it giving a short description of
the data it contains.

GDAL NetCDF Metadata
--------------------

All NetCDF attributes are transparently translated as GDAL metadata.

The translation follow these directives:

-  Global NetCDF metadata have a **NC_GLOBAL** tag prefixed.
-  Dataset metadata have their **variable name** prefixed.
-  Each prefix is followed by a **#** sign.
-  The NetCDF attribute follows the form: **name=value**.

Example:

::

   $ gdalinfo NETCDF:"sst.nc":tos
   Driver: netCDF/Network Common Data Format
   Size is 180, 170
   Coordinate System is `'
   Origin = (1.000000,-79.500000)
   Pixel Size = (1.98888889,0.99411765)
   Metadata:

NetCDF global attributes

::

     NC_GLOBAL#title=IPSL  model output prepared for IPCC Fourth Assessment SRES A2 experiment

Variables attributes for: tos, lon, lat and time

::

     tos#standard_name=sea_surface_temperature
     tos#long_name=Sea Surface Temperature
     tos#units=K
     tos#cell_methods=time: mean (interval: 30 minutes)
     tos#_FillValue=1.000000e+20
     tos#missing_value=1.000000e+20
     tos#original_name=sosstsst
     tos#original_units=degC
     tos#history= At   16:37:23 on 01/11/2005: CMOR altered the data in the following ways: added 2.73150E+02 to yield output units;  Cyclical dimension was output starting at a different lon;
     lon#standard_name=longitude
     lon#long_name=longitude
     lon#units=degrees_east
     lon#axis=X
     lon#bounds=lon_bnds
     lon#original_units=degrees_east
     lat#standard_name=latitude
     lat#long_name=latitude
     lat#units=degrees_north
     lat#axis=Y
     lat#bounds=lat_bnds
     lat#original_units=degrees_north
     time#standard_name=time
     time#long_name=time
     time#units=days since 2001-1-1
     time#axis=T
     time#calendar=360_day
     time#bounds=time_bnds
     time#original_units=seconds since 2001-1-1

Product specific behavior
--------------------------

Sentinel 5
++++++++++

.. versionadded:: 3.4

The most verbose metadata is reported in the ``json:ISO_METADATA``,
``json:ESA_METADATA``, ``json:EOP_METADATA``, ``json:QA_STATISTICS``,
``json:GRANULE_DESCRIPTION``, ``json:ALGORITHM_SETTINGS`` and ``json:SUPPORT_DATA``
metadata domains.

Can be discovered for example with:

::

    gdalinfo -mdd all -json S5P.nc


Creation Options
----------------

-  **FORMAT=[NC/NC2/NC4/NC4C]**: Set the NetCDF file format to use, NC
   is the default. NC2 is normally supported by recent NetCDF
   installations, but NC4 and NC4C are available if NetCDF was compiled
   with NetCDF-4 (and HDF5) support.
-  **COMPRESS=[NONE/DEFLATE]**: Set the compression to use. DEFLATE is
   only available if NetCDF has been compiled with NetCDF-4 support.
   NC4C format is the default if DEFLATE compression is used.

-  **ZLEVEL=[1-9]**: Set the level of compression when using DEFLATE
   compression. A value of 9 is best, and 1 is least compression. The
   default is 1, which offers the best time/compression ratio.

-  **WRITE_BOTTOMUP=[YES/NO]**: Set the y-axis order for export,
   overriding the order detected by the driver. NetCDF files are usually
   assumed "bottom-up", contrary to GDAL's model which is "north up".
   This normally does not create a problem in the y-axis order, unless
   there is no y axis geo-referencing. The default for this setting is
   YES, so files will be exported in the NetCDF default "bottom-up"
   order. For import see Configuration Option GDAL_NETCDF_BOTTOMUP
   below.

-  **WRITE_GDAL_TAGS=[YES/NO]**: Define if GDAL tags used for
   georeferencing (spatial_ref and GeoTransform) should be exported, in
   addition to CF tags. Not all information is stored in the CF tags
   (such as named datums and EPSG codes), therefore the driver exports
   these variables by default. In import the CF "grid_mapping" variable
   takes precedence and the GDAL tags are used if they do not conflict
   with CF metadata. In GDAL 4, spatial_ref will not be exported. The
   crs_wkt CF metatata attribute will be used instead.

-  **WRITE_LONLAT=[YES/NO/IF_NEEDED]**: Define if CF lon/lat variables
   are written to file. Default is YES for geographic SRS and NO for
   projected SRS. This is normally not necessary for projected SRS as
   GDAL and many applications use the X/Y dimension variables and CF
   projection information. Use of IF_NEEDED option creates lon/lat
   variables if the projection is not part of the CF-1.5 standard.

-  **TYPE_LONLAT=[float/double]**: Set the variable type to use for
   lon/lat variables. Default is double for geographic SRS and float for
   projected SRS. If lon/lat variables are written for a projected SRS,
   the file is considerably large (each variable uses X*Y space),
   therefore TYPE_LONLAT=float and COMPRESS=DEFLATE are advisable in
   order to save space.

-  **PIXELTYPE=[DEFAULT/SIGNEDBYTE]**: By setting this to SIGNEDBYTE, a
   new Byte file can be forced to be written as signed byte.

Creation of multidimensional files with CreateCopy() 2D raster API
------------------------------------------------------------------

Starting with GDAL 3.1, the preferred way of creating > 2D files is to use the
the :ref:`multidim_raster_data_model` API. However it is possible to create
such files with the 2D raster API using the CreateCopy() method (note that at
time of writing, this is not supported using the Create() method).

The ``NETCDF_DIM_EXTRA={dim1_name,...dimN_name}`` metadata item must be set on the
source dataset, where dim1_name is the name of the slowest varying dimension
and dimN_name the name of the fastest varying one.

For each extra dimension, the ``NETCDF_DIM_{dim_name}_DEF={dimension_size,netcdf_data_type}``
metadata item must be set where dimension_size is the size of the dimension
(number of samples along that dimension) and netcdf_data_type is the integer
value for the netCDF data type of the corresponding indexing variable. Among the most useful
data types:

- 4 for Int
- 5 for Float
- 6 for Double
- 10 for Int64

The ``NETCDF_DIM_{dim_name}_VALUES={value1,...valueN}`` is set to define the
values of the indexing variable corresponding to dimension.

``dim_name#attribute`` metadata items can also be set to define the attributes
of the indexing variable of the dimension.

Example of creation of a Time,Z,Y,X 4D file in Python:

.. code-block:: python

    # Create in-memory file with required metadata to define the extra >2D
    # dimensions
    size_z = 2
    size_time = 3
    src_ds = gdal.GetDriverByName('MEM').Create('', 4, 3, size_z * size_time)
    src_ds.SetMetadataItem('NETCDF_DIM_EXTRA', '{time,Z}')
    # 6 is NC_DOUBLE
    src_ds.SetMetadataItem('NETCDF_DIM_Z_DEF', f"{{{size_z},6}}")
    src_ds.SetMetadataItem('NETCDF_DIM_Z_VALUES', '{1.25,2.50}')
    src_ds.SetMetadataItem('Z#axis', 'Z')
    src_ds.SetMetadataItem('NETCDF_DIM_time_DEF', f"{{{size_time},6}}")
    src_ds.SetMetadataItem('NETCDF_DIM_time_VALUES', '{1,2,3}')
    src_ds.SetMetadataItem('time#axis', 'T')
    src_ds.SetGeoTransform([2,1,0,49,0,-1])

    # Create netCDF file
    gdal.GetDriverByName('netCDF').CreateCopy('out.nc', src_ds)


Configuration Options
---------------------

-  **GDAL_NETCDF_BOTTOMUP=[YES/NO]** : Set the y-axis order for import,
   overriding the order detected by the driver. This option is usually
   not needed unless a specific dataset is causing problems (which
   should be reported in GDAL trac).

-  **GDAL_NETCDF_VERIFY_DIMS=[YES/STRICT]** : Try to guess which dimensions
   represent the latitude and longitude only by their attributes (STRICT)
   or also by guessing the name (YES), default is YES.

-  **GDAL_NETCDF_IGNORE_XY_AXIS_NAME_CHECKS=[YES/NO]** : When a dimension
   has been identified as latitude or longitude by its attributes, check
   if its name also matches the convention, default is NO.

VSI Virtual File System API support
-----------------------------------

Since GDAL 2.4, and with Linux kernel >=4.3 and libnetcdf >=4.5, read
operations on /vsi file systems are supported.

NetCDF-4 groups support on reading (GDAL >= 3.0)
------------------------------------------------

The driver has undergone significant changes in GDAL 3.0 to support
NetCDF-4 groups on reading:

-  Explore recursively all nested groups to create the subdatasets list

-  Subdatasets in nested groups use the /group1/group2/.../groupn/var
   standard NetCDF-4 convention, except for variables in the root group
   which do not have a leading slash for backward compatibility with the
   NetCDF-3 driver

-  Global attributes of each nested group are also collected in the GDAL
   dataset metadata, using the same convention
   /group1/group2/.../groupn/NC_GLOBAL#attr_name, except for the root
   group which do not have a leading slash for backward compatibility

-  When searching for a variable containing auxiliary information on the
   selected subdataset, like coordinate variables or grid_mapping, we
   now also search in parent groups and their children as specified in
   `Support of groups in
   CF <https://github.com/cf-convention/cf-conventions/issues/144>`__

Multidimensional API support
----------------------------

.. versionadded:: 3.1

The netCDF driver supports the :ref:`multidim_raster_data_model` for reading and
creation operations.

The :cpp:func:`GDALGroup::GetGroupNames` method supports the following options:

- GROUP_BY=SAME_DIMENSION. If set, single-dimensional variables will be exposed
  as a "virtual" subgroup. This enables the user to get a clearer organization of
  variables, for example in datasets where variables belonging to different
  trajectories are indexed by different dimensions but mixed in the same netCDF
  group.

The :cpp:func:`GDALGroup::OpenGroup` method supports the following options:

- GROUP_BY=SAME_DIMENSION. See above description

The :cpp:func:`GDALGroup::GetMDArrayNames` method supports the following options:

- SHOW_ALL=YES/NO. Defaults to NO. If set to YES, all variables will be listed.
- SHOW_ZERO_DIM=YES/NO. Defaults to NO. If set to NO, variables with 0-dimension
  will not be listed.
- SHOW_COORDINATES=YES/NO. Defaults to YES. If set to NO, variables refererenced
  in the ``coordinates`` attribute of another variable will not be listed.
- SHOW_BOUNDS=YES/NO. Defaults to YES. If set to NO, variables refererenced
  in the ``bounds`` attribute of another variable will not be listed.
- SHOW_INDEXING=YES/NO. Defaults to YES. If set to NO,
  single-dimensional variables whose name is equal to the name of their indexing
  variable will not be listed.
- SHOW_TIME=YES/NO. Defaults to YES. If set to NO,
  single-dimensional variables whose ``standard_name`` attribute is "time"
  will not be listed.
- GROUP_BY=SAME_DIMENSION. If set, single-dimensional variables will not be listed

The :cpp:func:`GDALGroup::CreateMDArray` method supports the following options:

- NC_TYPE=NC_CHAR/NC_BYTE/NC_INT64/NC_UINT64: to overload the netCDF data type
  normally deduced from the GDAL data type passed to CreateMDArray().
  NC_CHAR can only be used for strings of a fixed size.
- BLOCKSIZE=size_dim0,size_dim1,...,size_dimN: to set the netCDF chunk size,
  as set by nc_def_var_chunking(). There must be exactly as many values as the
  number of dimensions passed to CreateMDArray()
- COMPRESS=DEFLATE: to ask for deflate compression
- ZLEVEL=number: DEFLATE compression level (1-9)
- CHECKSUM=YES/NO: Whether to turn on Fletcher32 checksums. Checksum generation
  requires chunking, and if no explicit chunking has been asked with the
  BLOCKSIZE option, a default one will be used. Defaults to NO.
- FILTER=filterid,param1,...,paramN: Define a filter (typically a compression
  method) used for writing. This should be a list of numeric values, separated
  by commas. The first value is the filter id (list of potential values at
  https://support.hdfgroup.org/services/contributions.html#filters) and following
  values are per-filter parameters. More details about netCDF-4 filter support at
  https://www.unidata.ucar.edu/software/netcdf/docs/md__Users_wfisher_Desktop_docs_netcdf-c_docs_filters.html

Driver building
---------------

This driver is compiled with the UNIDATA NetCDF library.

You need to download or compile the NetCDF library before configuring
GDAL with NetCDF support.

See `NetCDF GDAL wiki <http://trac.osgeo.org/gdal/wiki/NetCDF>`__ for
build instructions and information regarding HDF4, NetCDF-4 and HDF5.

See Also:
---------

-  :ref:`Vector side of the netCDF driver. <vector.netcdf>`
-  `NetCDF CF-1.5
   convention <http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html>`__
-  `NetCDF compiled
   libraries <http://www.unidata.ucar.edu/downloads/netcdf/index.jsp>`__
-  `NetCDF
   Documentation <http://www.unidata.ucar.edu/software/netcdf/docs/>`__
