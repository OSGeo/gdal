.. _rfc-75:

================================================================================
RFC 75: Multidimensional arrays
================================================================================

============== ============================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2019-May-24
Last updated:  2019-Jul-22
Status:        Implemented in GDAL 3.1
============== ============================

Summary
-------

This document describes the addition of read/write support for multidimensional
arrays, in particular of dimension 3 or above, in GDAL core and a few select drivers.

Motivation
----------

Multidimensional arrays (also known as hypercubes) are a way of mоdelling
spatio-temporal (time series of
2D raster) or spatio-vertical-temporal (2D + Z dimension + time dimension) data which
are becoming increasingly more available. GDAL current raster model is however strongly
2D oriented. A number of drivers, such as netCDF, HDF4, HDF5, work around that
limitation by using raster bands or subdatasets to expose muliple 2D slices of what
is intrinsically a N>2 Multidimensional dataset. It is desirable to have a
proper API, and driver support, to be able to expose those multidimensional
arrays as such, and be able to perform slice and trim operations on them.

That topic had already been discussed in the past, in particular in this
`mailing list thread <https://lists.osgeo.org/pipermail/gdal-dev/2017-October/047472.html>`_

Proposed changes
----------------

A lot of existing GDAL raster API are strongly 2D oriented. Rather than upgrading
all those API, and driver code, to be ready for N arbitrary dimensions, which would
be a enormous effort for the benefit of only a small fraction of drivers, we propose
to add a new dedicated API to support multidimensional arrays. We also want
to support hierarchical structure of data as found in the
`HDF5 format and data model <https://portal.opengeospatial.org/files/81716>`_.
This model can encompass the needs of other formats/drivers that have multidimensional
capabilities such as HDF4, netCDF, GRIB, WCS.
Therefore the proposed API will be strongly inspired by the API of the HDF5 library itself.

Data model
~~~~~~~~~~
The data model is described in:
https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/user/multidim_raster_data_model.rst

C++ API
~~~~~~~

New classes and methods will be added.
See https://github.com/rouault/gdal/blob/rfc75/gdal/gcore/gdal_priv.h#L1715

A new driver capability will be added for drivers supporting multidimensional
rasters:

::

    #define GDAL_DCAP_MULTIDIM_RASTER     "DCAP_MULTIDIM_RASTER"


A new open flag, ``GDAL_OF_MULTIDIM_RASTER``, for :cpp:func:`GDALOpenEx`
will be added. When this is specified, drivers supporting multidimensional
raster will return a root GDALGroup. Otherwise their current traditional 2D
mode will still be used.

New creation options metadata items are added to documents multidimensional dataset,
group, dimension, array and attribute creation options.

.. code-block:: c++

    /** XML snippet with multidimensional dataset creation options.
    * @since GDAL 3.1
    */
    #define GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST "DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST"

    /** XML snippet with multidimensional group creation options.
    * @since GDAL 3.1
    */
    #define GDAL_DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST "DMD_MULTIDIM_GROUP_CREATIONOPTIONLIST"

    /** XML snippet with multidimensional dimension creation options.
    * @since GDAL 3.1
    */
    #define GDAL_DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST "DMD_MULTIDIM_DIMENSION_CREATIONOPTIONLIST"

    /** XML snippet with multidimensional array creation options.
    * @since GDAL 3.1
    */
    #define GDAL_DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST "DMD_MULTIDIM_ARRAY_CREATIONOPTIONLIST"

    /** XML snippet with multidimensional attribute creation options.
    * @since GDAL 3.1
    */
    #define GDAL_DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST "DMD_MULTIDIM_ATTRIBUTE_CREATIONOPTIONLIST"

Examples with the netCDF driver:

.. code-block:: xml

    <MultiDimDatasetCreationOptionList>
    <Option name="FORMAT" type="string-select" default="NC4">
        <Value>NC</Value>
        <Value>NC2</Value>
        <Value>NC4</Value>
        <Value>NC4C</Value>
    </Option>
    <Option name="CONVENTIONS" type="string" default="CF-1.6" description="Value of the Conventions attribute" />
    </MultiDimDatasetCreationOptionList>


    <MultiDimDimensionCreationOptionList>
    <Option name="UNLIMITED" type="boolean" description="Whether the dimension should be unlimited" default="false" />
    </MultiDimDimensionCreationOptionList>


    <MultiDimArrayCreationOptionList>
    <Option name="BLOCKSIZE" type="int" description="Block size in pixels" />
    <Option name="COMPRESS" type="string-select" default="NONE">
        <Value>NONE</Value>
        <Value>DEFLATE</Value>
    </Option>
    <Option name="ZLEVEL" type="int" description="DEFLATE compression level 1-9" default="1" />
    <Option name="NC_TYPE" type="string-select" default="netCDF data type">
        <Value>AUTO</Value>
        <Value>NC_BYTE</Value>
        <Value>NC_INT64</Value>
        <Value>NC_UINT64</Value>
    </Option>
    </MultiDimArrayCreationOptionList>


    <MultiDimAttributeCreationOptionList>
    <Option name="NC_TYPE" type="string-select" default="netCDF data type">
        <Value>AUTO</Value>
        <Value>NC_BYTE</Value>
        <Value>NC_CHAR</Value>
        <Value>NC_INT64</Value>
        <Value>NC_UINT64</Value>
    </Option>
    </MultiDimAttributeCreationOptionList>


C API
~~~~~

All C++ methods are mapped to the C API.
See https://github.com/rouault/gdal/blob/rfc75/gdal/gcore/gdal.h#L1397

Driver changes
~~~~~~~~~~~~~~

- The MEM driver will implement read and write support.
- The VRT driver will allow extraction of 2D slices from multidimensional
  drivers to 2D/classic drivers, as well as multidimensional->multidimensional
  slicing/trimming
- The netCDF driver will implement read and write support.
- The HDF4 and HDF5 drivers will implement read support.
- The GRIB driver will implement read support (exposing X,Y,Time arrays for GRIB
  messages only differing by timestamp)

New Utilities
~~~~~~~~~~~~~

- A new gdalmdiminfo utility is added to report the hierarchical structure and content.
  Its output format is JSON. See https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/programs/gdalmdiminfo.rst
  for its documentation.

- A new gdalmdimtranslate utility is added to convert multidimensional raster between
  different formats, and/or can perform selective conversion of specific arrays
  and groups, and/or subsetting operations. It can also do extraction of 2D slices
  from multidimensional drivers to 2D/classic drivers.
  See https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/programs/gdalmdimtranslate.rst
  for its documentation.

SWIG binding changes
~~~~~~~~~~~~~~~~~~~~

The C API is mapped to the SWIG bindings. The scope is complete for the
Python bindings. Other languages would need to add missing typemaps, but this
is not in the scope of the work of this RFC.
For Python bindings, NumPy integration is done.

Limitations
-----------

This is intended to be a preliminary work on that topic. While the aim is for it
to be be usable for the defined scope, it will probably require future
enhancements to fill functional and/or performance gaps.

- No block cache mechanism (not sure this is needed)
- No sub-pixel requests, or non-nearest subsampling
- Upgrade of WCS driver or other drivers with potential multidimensional
  capabilities are not part of this RFC.
- SWIG bindings: full scope only for Python bindings.

Backward compatibility
----------------------

No backward incompatibility. Only API and utility additions.

Documentation
-------------

- Data model: https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/user/multidim_raster_data_model.rst
- API tutorial: https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/tutorials/multidimensional_api_tut.rst
- gdalmdiminfo: https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/programs/gdalmdiminfo.rst
- gdalmdimtranslate: https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/programs/gdalmdimtranslate.rst
- VRT driver: https://github.com/rouault/gdal/blob/rfc75/gdal/doc/source/drivers/raster/vrt_multidimensional.rst

Testing
-------

The gdalautotest suite is extended to test the modified drivers and the new
utilities.

Implementation
--------------

The implementation will be done by Even Rouault.
A preliminary implementation is available at
https://github.com/OSGeo/gdal/pull/1704

Voting history
--------------

+1 from HowardB, NormanB and EvenR
