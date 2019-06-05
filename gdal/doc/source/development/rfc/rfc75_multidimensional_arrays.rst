.. _rfc-75:

================================================================================
RFC 75: Multidimensional arrays
================================================================================

============== ============================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2019-May-24
Status:        Work in progress
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

A GDALDataset with mulidimensional content will contain a root GDALGroup.

A GDALGroup (modelling a `HDF5 Group <https://portal.opengeospatial.org/files/81716#_hdf5_group>`_)
is a named container of GDALAttribute, GDALMDArray or
other GDALGroup. Hence GDALGroup can describe a hierarchy of objects.

A GDALAttribute (modelling a `HDF5 Attribute <https://portal.opengeospatial.org/files/81716#_hdf5_attribute>`_)
has a name and a value, and is typically used to describe a metadata item.
The value can be (for the HDF5 format) in the general case a multidimensional array
of "any" type (in most cases, this will be a single value of string or numeric type)

A GDALMDArray (modelling a `HDF5 Dataset <https://portal.opengeospatial.org/files/81716#_hdf5_dataset>`_)
has a name, a multidimensional array and a list of GDALAttribute.

A GDALDimension describes a dimension / axis used to index multidimensional arrays.
It has a name, a size (that is the number of values that can be indexed along
the dimension), and can optionaly point to a GDALMDArray variable, typically
one-dimensional, describing the values taken by the dimension.
For a georeferenced GDALMDArray and its X dimension, this will be typically
the values of the easting/longitude for each grid point.

A GDALExtendedDataType (modelling a `HDF5 datatype <https://portal.opengeospatial.org/files/81716#_hdf5_datatype>`_)
describes the type taken by an individual value of
a GDALAttribute or GDALMDArray. Its class can be NUMERIC,
STRING or COMPOUND. For NUMERIC, the existing :cpp:enum:`GDALDataType` enumerated
values are supported. For COMPOUND, the data type is a list of members, each
member being described by a name and a GDALExtendedDataType.

.. note::

   The HDF5 modelisation allows for more complex datatypes, but for now, we
   will stand with the above restrictions, which should be sufficient to cover
   most practical use cases.

.. note::

    HDF5 does not have native data types for complex values whereas
    GDALDataType does. So the driver may decide to expose a GDT\_Cxxxx datatype
    from a HDF5 Compound data type representing a complex value.

Differences with the GDAL 2D raster data model:

- the concept of GDALRasterBand is no longer used for multidimensional.
  This can be modelled as either different GDALMDArray, or using a compound
  data type.


C++ API
~~~~~~~

A new driver capability will be added for drivers supporting multidimensional
rasters:

::

    #define GDAL_DCAP_MULTIDIM_RASTER     "DCAP_MULTIDIM_RASTER"


A new open flag, ``GDAL_OF_MULTIDIM_RASTER``, for :cpp:func:`GDALOpenEx`
will be added. When this is specified, drivers supporting multidimensional
raster will return a root GDALGroup. Otherwise their current traditionnal 2D
mode will still be used.

The following new classes and methods will be added:

.. code-block:: c++

    class GDALDriver (class already exists)
    - CreateMultiDimensional(name: string, options: const char* const *) -> GDALDataset with an empty root GDALGroup
    - CreateCopy() implementations of compatible drivers should be able to process source datasets with multidimensional arrays

    class GDALDataset (class already exists)
     - virtual GetRootGroup() --> GDALGroup when opened in mulidimensional mode, nullptr otherwise

    // models a HDF5 Group
    class GDALGroup
    - GetName()
    - virtual GetMDArrayNames() -> list of string
    - virtual OpenMDArray(string) -> unique_ptr<GDALMDArray>
    - virtual GetGroupNames() -> list of string
    - virtual OpenGroup(string) -> unique_ptr<GDALGroup>
    - virtual GetAttributes() -> array of GDALAttribute*
    // Write support
    - virtual CreateGroup(name, options: const char* const *) -> GDALGroup
    - virtual CreateMDArray(name: string,
                    dimensions: vector<shared_ptr<GDALDimension>>,
                    data_type GDALExtendedDataType,
                    options: const char* const *) -> GDALMDArray
    - virtual CreateAttribute(name: string,
                      dimensions: vector<uint64_t>,
                      data_type GDALExtendedDataType,
                      options: const char* const *) -> GDALAttribute

    // base class for GDALAttribute and GDALMDArray
    abstract class GDALAbstractMDArray:
    - GetName()
    - GetDimensionCount() -> size_t
    - GetDimensions() -> array of shared_ptr<GDALDimension>
    - GetDataType() -> GDALExtendedDataType
    - virtual Read(const uint64_t* array_start_idx,     // array of size GetDimensionCount()
           const size_t* count,                 // array of size GetDimensionCount()
           const uint64_t* array_stride,        // stride in elements
           const std::ptrdiff_t* buffer_stride, // stride in elements
           GDALExtendedDataType buffer_datatype,
           void* dst_buffer)
    - virtual Write(const uint64_t* array_start_idx,    // array of size GetDimensionCount()
           const size_t* count,                 // array of size GetDimensionCount()
           const uint64_t* array_stride,        // stride in elements
           const std::ptrdiff_t* buffer_stride, // stride in elements
           GDALExtendedDataType buffer_datatype,
           const void* src_buffer)

    // models a HDF5 Attribute
    class GDALAttribute: GDALAbstractMDArray
    - virtual ReadAsString()
    - virtual ReadAsDouble()
    - virtual ReadAsStringArray()
    - virtual ReadAsDoubleArray()
    - virtual Write(string)
    - virtual Write(int)
    - virtual Write(double)

    // models a HDF5 Dataset
    class GDALMDArray: GDALAbstractMDArray
    - virtual GetAttributes() --> array of GDALAttribute*
    - nodata, scale, offset, unit, ...
    - virtual GetSpatialRef()
    // Write support
    - virtual CreateAttribute(name: string,
                      dim_count: uint32_t,
                      dim_sizes: uint64_t[],
                      data_type GDALExtendedDataType,
                      options: const char* const *) -> GDALAttribute

    // models a HDF5 Datatype, simplified
    class GDALExtendedDataType:
    - GetClass() -> COMPOUND, NUMERIC, STRING
    - GetNumericDataType() -> GDALDataType for NUMERIC
    - GetComponents() -> for COMPOUND, array of [ name, offset, 
    GDALExtendedDataType ]

    // models a netCDF dimension. Generic HDF5 may not have all attributes populated
    class GDALDimension:
    - string name. e.g "X", "Y", "Z", "T"
    - uint64_t size
    - GDALMDArray indexing_variable: optional. Variable with the values 
        taken by the dimension
    - static Create(name, size, indexing_variable, option) -> shared_ptr<GDALDimension>

C API
~~~~~

TODO

Driver changes
~~~~~~~~~~~~~~

- The VRT driver will allow extraction of 2D slices from multidimensional
  drivers to 2D/classic drivers, as well as multidimensional->multidimensional
  slicing/trimming
- The netCDF driver will implement read and write support.
- The HDF4 and HDF5 drivers will implement read support.
- The GRIB driver will implement read support (exposing X,Y,Time arrays for GRIB
  messages only differing by timestamp)

Utility changes
~~~~~~~~~~~~~~~

- gdalinfo will report the hierachical structure
- gdal_translate will extraction of 2D slices from multidimensional drivers
  to 2D/classic drivers, or multidimensional->multidimensional slicing/trimming
  
At that point, I'm not sure if it is better to extend existing utilities
(possibly with a switch to enable multidimensional mode) or have new dedicated
utilities: gdalmdinfo / gdalmdtranslate. Especially for gdal_translate where a
big number of options will not apply for the multidimensional case.

SWIG binding changes
~~~~~~~~~~~~~~~~~~~~

TODO.
Mapping of C API.
For Python bindings, NumPy integration with ndarray.

Limitations
-----------

This is intended to be a preliminary work on that topic. While the aim is for it
to be be usable for the defined scop, it will probably require future
enhancements to fill functional and/or performance gaps.

Limitations I can think of currently are:

- No block cache mechanism (not sure this is needed)
- No sub-pixel requests, or non-nearest subsampling
- Upgrade of WCS driver or other drivers with potential multidimensional
  capabilities are not part of this RFC.
- SWIG bindings: if new typemaps needed, only be implemented for Python bindings

Backward compatibility
----------------------

At that point, no backward incompatibility anticipated.

Documentation
-------------

TODO, including a new document describing the multidimensional data model.

Testing
-------

TODO

Implementation
--------------

The implementation will be done by Even Rouault. TODO

Voting history
--------------

TBD
