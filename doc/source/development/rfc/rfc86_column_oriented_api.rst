.. _rfc-86:

=============================================================
RFC 86: Column-oriented read API for vector layers
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2022-May-24
Updated:       2022-June-14
Status:        Adopted
Target:        GDAL 3.6
============== =============================================

Summary
-------

This RFC describes the addition of new methods to the :cpp:class:`OGRLayer` class to retrieve
batches of features with a column-oriented memory layout, that suits formats that
have that organization or downstream consumers that expect data to be presented
in such a way, in particular the `Apache Arrow <https://arrow.apache.org/docs/>`_,
`Pandas <https://pandas.pydata.org/>`_ / `GeoPandas <https://geopandas.org/>`_
ecosystem, R spatial packages, and many modern (data analytics focused)
databases / engines which are column oriented (eg Snowflake, Google BigQuery, ..)

Motivation
----------

Currently, to retrieve feature information, users must iterate over each feature
of a layer with GetNextFeature(), which returns a C++ object, on which query
attributes and geometries are retrieved with various "get" methods. When invoked
from binding languages, a overhead typically occurs each time the other language
calls native code. So to retrieve all information on a layer made of N_features
and N_fields, you need of the order of N_features * N_fields calls.
That overhead is significant. See below :ref:`rfc-86-benchmarks`.

Another inconvenience of the C API is that processings that involve many rows
of a same field (e.g computing statistics on a field) may require data to be
contiguously placed in RAM, for the most efficient processing (use of vectorized
CPU instruction). The current OGR API does not allow that directly, and require
the users to shuffle itself data into appropriate data structures.
Similarly the above mentioned frameworks (Arrow, Pandas/GeoPandas) require
such memory layouts, and currently require reorganizing data when reading from OGR.
The `pyogrio <https://github.com/geopandas/pyogrio>`_ project is for example
an attempt at addressing that need.

Furthermore, the :ref:`vector.arrow` and :ref:`vector.parquet` drivers,
whose file organization is columnar, and batch oriented, have been added in GDAL 3.5.0.
Consequently a columnar-oriented API will enable the best performance
for those formats.

Details
-------

The new proposed API implements the
`Apache Arrow C Stream interface <https://arrow.apache.org/docs/format/CStreamInterface.html>`_.
Reading that document, as well of the first paragraphs of the
`Apache Arrow C data interface <https://arrow.apache.org/docs/format/CDataInterface.html>`_.
(details on the various data types can be skipped) is strongly encouraged for a
better understanding of the rest of this RFC.

The Arrow C Stream interface is currently marked as experimental, but it has not
evolved since its introduction in Nov 2020 and is already used in ABI sensitive
places like the interface between the Arrow R bindings and DuckDB.

This interface consists of a set of C structures, ArrowArrayStream, that provides
two main callbacks to get:

- a ArrowSchema with the get_schema() callback. A ArrowSchema describes a set of
  field descriptions (name, type, metadata). All OGR data types have a corresponding
  Arrow data type.

- a sequence of ArrowArray with the get_next() callback. A ArrowArray captures
  a set of values for a specific column/field in a subset of features.
  This is the equivalent of a
  `Series <https://arrow.apache.org/docs/python/pandas.html#series>`_ in a Pandas DataFrame.
  This is a potentially hiearchical structure that can aggregate
  sub arrays, and in OGR usage, the main array will be a StructArray which is
  the collection of OGR attribute and geometry fields.
  The layout of buffers and children arrays per data type is detailed in the
  `Arrow Columnar Format <https://arrow.apache.org/docs/format/Columnar.html>`_.

If a layer consists of 4 features with 2 fields (one of integer type, one of
floating-point type), the representation as a ArrowArray is *conceptually* the
following one:

.. code-block:: c

    array.children[0].buffers[1] = { 1, 2, 3, 4 };
    array.children[1].buffers[1] = { 1.2, 2.3, 3.4, 4.5 };

The content of a whole layer can be seen as a sequence of record batches, each
record batches being an ArrowArray of a subset of features. Instead of iterating
over individual features, one iterates over a batch of several features at
once.

The ArrowArrayStream, ArrowSchema, ArrowArray structures are defined in a ogr_recordbatch.h
public header file, directly derived from https://github.com/apache/arrow/blob/master/cpp/src/arrow/c/abi.h
to get API/ABI compatibility with Apache Arrow C++. This header file must be
explicitly included when the related array batch API is used.

The following virtual method is added to the OGRLayer class:

  .. code-block:: cpp

        virtual bool OGRLayer::GetArrowStream(struct ArrowArrayStream* out_stream,
                                              CSLConstList papszOptions = nullptr);

This method is also available in the C API as OGR_L_GetArrowStream().

out_stream is a pointer to a ArrowArrayStream structure, that can be in a uninitialized
state (the method will ignore any initial content).

On successful return, and when the stream interfaces is no longer needed, it must must
be freed with out_stream->release(out_stream).

There are extra precautions to take into account in a OGR context. Unless
otherwise specified by a particular driver implementation, the ArrowArrayStream
structure, and the ArrowSchema or ArrowArray objects its callbacks have returned,
should no longer be used (except for potentially being released) after the
OGRLayer from which it was initialized has been destroyed (typically at dataset
closing). Furthermore, unless otherwise specified by a particular driver
implementation, only one ArrowArrayStream can be active at a time on
a given layer (that is the last active one must be explicitly released before
a next one is asked). Changing filter state, ignored columns, modifying the schema
or using ResetReading()/GetNextFeature() while using a ArrowArrayStream is
strongly discouraged and may lead to unexpected results. As a rule of thumb,
no OGRLayer methods that affect the state of a layer should be called on a
layer, while an ArrowArrayStream on it is active.

A potential usage can be:

.. code-block:: cpp

    struct ArrowArrayStream stream;
    if( !poLayer->GetArrowStream(&stream, nullptr))
    {
        fprintf(stderr, "GetArrowStream() failed\n");
        exit(1);
    }
    struct ArrowSchema schema;
    if( stream.get_schema(&stream, &schema) == 0 )
    {
        // Do something useful
        schema.release(schema);
    }
    while( true )
    {
        struct ArrowArray array;
        // Look for an error (get_next() returning a non-zero code), or
        // end of iteration (array.release == nullptr)
        //
        if( stream.get_next(&stream, &array) != 0 ||
            array.release == nullptr )
        {
            break;
        }
        // Do something useful
        array.release(&array);
    }
    stream.release(&stream);

The papszOptions that may be provided is a NULL terminated list of key=value
strings, that may be driver specific.

OGRLayer has a base implementation of GetArrowStream() that is such:

- The get_schema() callback returns a schema whose top-level object returned is
  of type Struct, and whose children consist in the FID column, all OGR attribute
  fields and geometry fields to Arrow fields.
  The FID column may be omitted by providing the INCLUDE_FID=NO option.

  When get_schema() returns 0, and the schema is no longer needed, it must
  be released with the following procedure, to take into account that it might
  have been released by other code, as documented in the Arrow C data
  interface:

  .. code-block:: c

          if( out_schema->release )
              out_schema->release(out_schema)


- The get_next() callback retrieve the next record batch over the layer.

  out_array is a pointer to a ArrowArray structure, that can be in a uninitialized
  state (the method will ignore any initial content).

  The default implementation uses GetNextFeature() internally to retrieve batches
  of up to 65,536 features (configurable with the MAX_FEATURES_IN_BATCH=num option).
  The starting address of buffers allocated by the
  default implementation is aligned on 64-byte boundaries.

  The default implementation outputs geometries as WKB in a binary field,
  whose corresponding entry in the schema is marked with the metadata item
  ``ARROW:extension:name`` set to ``ogc.wkb``. Specialized implementations may output
  by default other formats (particularly the Arrow driver that can return geometries
  encoded according to the GeoArrow specification (using a list of coordinates).
  The GEOMETRY_ENCODING=WKB option can be passed to force the use of WKB (through
  the default implementation)

  The method may take into account ignored fields set with SetIgnoredFields() (the
  default implementation does), and should take into account filters set with
  SetSpatialFilter() and SetAttributeFilter(). Note however that specialized implementations
  may fallback to the default (slower) implementation when filters are set.

  Mixing calls to GetNextFeature() and get_next() is not recommended, as
  the behaviour will be unspecified (but it should not crash).

  When get_next() returns 0, and the array is no longer needed, it must
  be released with the following procedure, to take into account that it might
  have been released by other code, as documented in the Arrow C data
  interface:

  .. code-block:: c

          if( out_array->release )
              out_array->release(out_array)

Drivers that have a specialized implementation should advertize the
new OLCFastGetArrowStream layer capability.

Other remarks
-------------

Using directly (as a producer or a consumer) a ArrowArray is admitedly not
trivial, and requires good intimacy with the Arrow C data interface and columnar
array specifications, to know, in which buffer of an array, data is to be read,
which data type void* buffers should be cast to, how to use buffers that contain
null/not_null information, how to use offset buffers for data types of type List, etc.

For the consuming side, the new API will be best used with the (Py)Arrow, Pandas,
GeoPandas, Numpy libraries which offer easier and safer access to record batches.
The study of the gdal_array._RecordBatchAsNumpy() method added to the SWIG Python
bindings can give a good hint of how to use an ArrowArray object, in conjunction
with the associated ArrowSchema. DuckDB is also another example of using the ArrowArray
inferface: https://github.com/duckdb/duckdb/blob/master/src/common/types/data_chunk.cpp

It is not expected that most drivers will have a dedicated implementation of
GetArrowStream() or its callbacks. Implementing it requires a non-trivial effort, and
significant gains are to be expected only for those for which I/O is very fast,
and thus in-memory shuffling of data takes a substantial time relatively to the
total time (I/O + shuffling).

Potential future work, not in the scope of this RFC, could be the addition of a
column-oriented method to write new features, a WriteRecordBatch() method.

Impacted drivers
----------------

- Arrow and Parquet: get_schema() and get_next() have a
  specialized implementation in those drivers that directly map to methods of
  the arrow-cpp library that bridges at near zero cost (no data copying) the
  internal C++ implementation with the C data interface.

- FlatGeoBuf and GeoPackage: a specialized implementation of get_next() has been done,
  which saves going through the OGRFeature abstraction. See below benchmarks for
  measurement of the efficiency.

Bindings
--------

Per this RFC, only the Python bindings are extended to map the new functionality.

The ogr.Layer class receives the following new methods:

- GetArrowStreamAsPyArrow(): wrapper over OGRLayer::GetArrowStream() that
  has a ``schema`` property with the C ArrowSchema into a corresponding
  PyArrow Schema object and which implements a Python iterator exposing the
  C ArrowArray returned by the get_next() callback as a corresponding
  PyArrow Array object. This is a almost zero-cost call.

- GetArrowStreamAsNumPy(): wrapper over OGRLayer::GetArrowStream()
  which implements a Python iterator exposing the C ArrowArray returned by the
  get_next() callback as a Python dictionary whose keys are field names and
  values a Numpy array representing the values of the ArrowArray. The mapping of
  types is done for all Arrow data types returned by the base implementation of
  OGRLayer::GetArrowStream(), but may not cover "exotic" data types that can
  be returned by specialized implementations such as the one in the Arrow/Parquet
  driver. For numeric data types, the Numpy array is a zero-copy adaptation of the
  C buffer. For other data types, a copy is involved, with potentially arrays of
  Python objects.


.. _rfc-86-benchmarks:

Benchmarks
----------

The test programs referenced in :ref:`rfc-86-annexes` have been run on a
dataset with 3.3 millions features, with 13 fields each (2 fields of type Integer,
8 of type String, 3 of type DateTime) and polygon geometries.

:ref:`rfc-86-bench-ogr-py`, :ref:`rfc-86-bench-fiona` and :ref:`rfc-86-bench-ogr-cpp`
have similar functionality: iterating over features with GetNextFeature().

:ref:`rfc-86-bench-pyogrio-raw` does a little more by building Arrow arrays.

:ref:`rfc-86-bench-pyogrio`, :ref:`rfc-86-bench-geopandas` and :ref:`rfc-86-bench-ogr-to-geopandas`
have all similar functionality: building a GeoPandas GeoDataFrame

:ref:`rfc-86-bench-ogr-batch-cpp` can be used to measure the raw performance of the
proposed GetArrowStream() API.

1. nz-building-outlines.fgb (FlatGeoBuf, 1.8 GB)

========================================  ============
        Bench program                      Timing (s)
========================================  ============
bench_ogr.cpp                             6.3
bench_ogr.py                              71
bench_fiona.py                            68
bench_pyogrio_raw.py                      40
bench_pyogrio.py                          108
bench_geopandas.py                        232
bench_ogr_batch.cpp (driver impl.)        4.5
bench_ogr_batch.cpp (base impl.)          14
bench_ogr_to_geopandas.py (driver impl.)  10
bench_ogr_to_geopandas.py (base impl.)    20
========================================  ============

"driver impl." means that the specialized implementation of GetArrowStream()
is used.
"base impl." means that the generic implementation of GetArrowStream(),
using GetNextFeature() underneath, is used.

2. nz-building-outlines.parquet (GeoParquet, 436 MB)

========================================  ============
        Bench program                      Timing (s)
========================================  ============
bench_ogr.cpp                             6.4
bench_ogr.py                              72
bench_fiona.py                            70
bench_pyogrio_raw.py                      46
bench_pyogrio.py                          115
bench_geopandas.py                        228
bench_ogr_batch.cpp (driver impl.)        1.6
bench_ogr_batch.cpp (base impl.)          13.8
bench_ogr_to_geopandas.py (driver impl.)  6.8
bench_ogr_to_geopandas.py (base impl.)    20
========================================  ============

Note: Fiona slightly modified to accept Parquet driver as a recognized one.

3. nz-building-outlines.gpkg (GeoPackage, 1.7 GB)

========================================  ============
        Bench program                      Timing (s)
========================================  ============
bench_ogr.cpp                             7.6
bench_ogr.py                              71
bench_fiona.py                            63
bench_pyogrio_raw.py                      41
bench_pyogrio.py                          103
bench_geopandas.py                        227
bench_ogr_batch.cpp (driver impl.)        1.0
bench_ogr_batch.cpp (base impl.)          15.5
bench_ogr_to_geopandas.py (driver impl.)  10
bench_ogr_to_geopandas.py (base impl.)    21
========================================  ============

bench_ogr_batch.cpp is faster on GeoPackage than on FlatGeoBuf, because the
GeoPackage geometry encoding is already in WKB (with an extra header), while
FlatGeoBuf uses a different encoding.

Note: it is not fully understood why bench_ogr_batch.cpp is faster with
GeoPackage compared to GeoParquet while being slower in bench_ogr_to_geopandas.
It might potentially be due to Parquet batches being slices of larger arrays,
and pa.RecordBatch.from_arrays() being able to merge them faster.


This demonstrates that:

- the new API can yield signficant performance gains to
  ingest a OGR layer as a GeoPandas GeoDataFrame, of the order of a 4x - 10x
  speed-up compared to pyogrio, even without a specialized implementation of
  GetArrowStream(), and with formats that have a natural row organization
  (FlatGeoBuf, GeoPackage).

- the Parquet driver is where this shines most due to the file organization being
  columnar, and its native access layer being ArrowArray compatible.

- for drivers that don't have a specialized implementation of GetArrowStream()
  and whose layout is row oriented, the GetNextFeature() approach is
  (a bit) faster than GetArrowStream().

Backward compatibility
----------------------

Only API additions, fully backward compatible.

The C++ ABI changes due to the addition of virtual methods.

New dependencies
----------------

- For libgdal: none

  The Apache Arrow C data interface just defines 2 C structures. GDAL itself
  does not need to link against the Apache Arrow C++ libraries (it might link
  against them, if the Arrow and/or Parquet drivers are enabled, but that's orthogonal
  to the topic discussed in this RFC).

- For Python bindings: none at compile time. At runtime, pyarrow is imported
  by GetArrowStreamAsPyArrow().
  The GetArrowStreamAsNumPy() method is implemented internaly by the
  gdal_array module, and thus is only available if Numpy is available at compile time
  and runtime.

Documentation
-------------

New methods are documented, and a new documentation page will be added in the
documentation.

Testing
-------

New methods are tested.

Related PRs:
-------------

https://github.com/OSGeo/gdal/compare/master...rouault:arrow_batch_new?expand=1

.. _rfc-86-annexes:

Annexes
-------

.. _rfc-86-bench-ogr-cpp:

bench_ogr.cpp
+++++++++++++

Use of traditional GetNextFeature() and related API from C

.. code-block:: cpp

    #include "gdal_priv.h"
    #include "ogr_api.h"
    #include "ogrsf_frmts.h"

    int main(int argc, char* argv[])
    {
        GDALAllRegister();
        GDALDataset* poDS = GDALDataset::Open(argv[1]);
        OGRLayer* poLayer = poDS->GetLayer(0);
        OGRLayerH hLayer = OGRLayer::ToHandle(poLayer);
        OGRFeatureDefnH hFDefn = OGR_L_GetLayerDefn(hLayer);
        int nFields = OGR_FD_GetFieldCount(hFDefn);
        std::vector<OGRFieldType> aeTypes;
        for( int i = 0; i < nFields; i++ )
            aeTypes.push_back(OGR_Fld_GetType(OGR_FD_GetFieldDefn(hFDefn, i)));
        int nYear, nMonth, nDay, nHour, nMin, nSecond, nTZ;
        while( true )
        {
            OGRFeatureH hFeat = OGR_L_GetNextFeature(hLayer);
            if( hFeat == nullptr )
                break;
            OGR_F_GetFID(hFeat);
            for( int i = 0; i < nFields; i++ )
            {
                if( aeTypes[i] == OFTInteger )
                    OGR_F_GetFieldAsInteger(hFeat, i);
                else if( aeTypes[i] == OFTInteger64 )
                    OGR_F_GetFieldAsInteger64(hFeat, i);
                else if( aeTypes[i] == OFTReal )
                    OGR_F_GetFieldAsDouble(hFeat, i);
                else if( aeTypes[i] == OFTString )
                    OGR_F_GetFieldAsString(hFeat, i);
                else if( aeTypes[i] == OFTDateTime )
                    OGR_F_GetFieldAsDateTime(hFeat, i, &nYear, &nMonth, &nDay, &nHour, &nMin, &nSecond, &nTZ);
            }
            OGRGeometryH hGeom = OGR_F_GetGeometryRef(hFeat);
            if( hGeom )
            {
                int size = OGR_G_WkbSize(hGeom);
                GByte* pabyWKB = static_cast<GByte*>(malloc(size));
                OGR_G_ExportToIsoWkb( hGeom, wkbNDR, pabyWKB);
                CPLFree(pabyWKB);
            }
            OGR_F_Destroy(hFeat);
        }
        delete poDS;
        return 0;
    }

.. _rfc-86-bench-ogr-py:

bench_ogr.py
++++++++++++

Use of traditional GetNextFeature() and related API from Python (port of bench_ogr.cpp)

.. code-block:: python

    from osgeo import ogr
    import sys

    ds = ogr.Open(sys.argv[1])
    lyr = ds.GetLayer(0)
    lyr_defn = lyr.GetLayerDefn()
    fld_count = lyr_defn.GetFieldCount()
    types = [lyr_defn.GetFieldDefn(i).GetType() for i in range(fld_count)]
    for f in lyr:
        f.GetFID()
        for i in range(fld_count):
            fld_type = types[i]
            if fld_type == ogr.OFTInteger:
                f.GetFieldAsInteger(i)
            elif fld_type == ogr.OFTReal:
                f.GetFieldAsDouble(i)
            elif fld_type == ogr.OFTString:
                f.GetFieldAsString(i)
            else:
                f.GetField(i)
        geom = f.GetGeometryRef()
        if geom:
            geom.ExportToWkb()

.. _rfc-86-bench-fiona:

bench_fiona.py
++++++++++++++

Use of the Fiona Python library which uses the OGR C GetNextFeature() underneath to
expose them as GeoJSON features holded by a Python dictionary.

.. code-block:: python

    import sys
    import fiona

    with fiona.open(sys.argv[1], 'r') as features:
        for f in features:
            pass

.. note:: Changing the above loop to ``list(features)`` to accumulate features has
          a significant negative impact on memory usage on big datasets, and on
          memory usage.

.. _rfc-86-bench-pyogrio-raw:

bench_pyogrio_raw.py
++++++++++++++++++++

Use of the pyogrio Python library which uses the OGR C GetNextFeature() underneath to
expose a layer as a set of Arrow arrays.

.. code-block:: python

    import sys
    from pyogrio.raw import read

    read(sys.argv[1])


.. _rfc-86-bench-pyogrio:

bench_pyogrio.py
++++++++++++++++

Use of the pyogrio Python library which uses the OGR C GetNextFeature() underneath to
expose a layer as GeoPandas GeoDataFrame (which involves parsing WKB as GEOS objects)

.. code-block:: python

    import sys
    from pyogrio import read_dataframe

    read_dataframe(sys.argv[1])

.. _rfc-86-bench-geopandas:

bench_gepandas.py
+++++++++++++++++

Use of the GeoPandas Python library which uses Fiona underneath to
expose a layer as GeoPandas GeoDataFrame.

.. code-block:: python

    import sys
    import geopandas

    gdf = geopandas.read_file(sys.argv[1])

.. _rfc-86-bench-ogr-batch-cpp:

bench_ogr_batch.cpp
+++++++++++++++++++

Use of the proposed GetNextRecordBatch() API from C++

.. code-block:: cpp

    #include "gdal_priv.h"
    #include "ogr_api.h"
    #include "ogrsf_frmts.h"
    #include "ogr_recordbatch.h"

    int main(int argc, char* argv[])
    {
        GDALAllRegister();
        GDALDataset* poDS = GDALDataset::Open(argv[1]);
        OGRLayer* poLayer = poDS->GetLayer(0);
        OGRLayerH hLayer = OGRLayer::ToHandle(poLayer);
        struct ArrowArrayStream stream;
        if( !OGR_L_GetArrowStream(hLayer, &stream, nullptr))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "OGR_L_GetArrowStream() failed\n");
            exit(1);
        }
        while( true )
        {
            struct ArrowArray array;
            if( stream.get_next(&stream, &array) != 0 ||
                array.release == nullptr )
            {
                break;
            }
            array.release(&array);
        }
        stream.release(&stream);
        delete poDS;
        return 0;
    }

.. _rfc-86-bench-ogr-to-geopandas:

bench_ogr_to_geopandas.py
+++++++++++++++++++++++++

Use of the proposed GetNextRecordBatchAsPyArrow API from Python, to build a
GeoPandas GeoDataFrame from the concatenation of the returned arrays.

.. code-block:: python

    import sys
    from osgeo import ogr
    import pyarrow as pa

    def layer_as_geopandas(lyr):
        stream = lyr.GetArrowStreamAsPyArrow()
        schema = stream.schema

        geom_field_name = None
        for field in schema:
            field_md = field.metadata
            if (field_md and field_md.get(b'ARROW:extension:name', None) == b'WKB') or field.name == lyr.GetGeometryColumn():
                geom_field_name = field.name
                break

        fields = [field for field in schema]
        schema_without_geom = pa.schema(list(filter(lambda f: f.name != geom_field_name, fields)))
        batches_without_geom = []
        non_geom_field_names = [f.name for f in filter(lambda f: f.name != geom_field_name, fields)]
        if geom_field_name:
            schema_geom = pa.schema(list(filter(lambda f: f.name == geom_field_name, fields)))
            batches_with_geom = []
        for record_batch in stream:
            arrays_without_geom = [record_batch.field(field_name) for field_name in non_geom_field_names]
            batch_without_geom = pa.RecordBatch.from_arrays(arrays_without_geom, schema=schema_without_geom)
            batches_without_geom.append(batch_without_geom)
            if geom_field_name:
                batch_with_geom = pa.RecordBatch.from_arrays([record_batch.field(geom_field_name)], schema=schema_geom)
                batches_with_geom.append(batch_with_geom)

        table = pa.Table.from_batches(batches_without_geom)
        df = table.to_pandas()
        if geom_field_name:
            from geopandas.array import from_wkb
            import geopandas as gp
            geometry = from_wkb(pa.Table.from_batches(batches_with_geom)[0])
            gdf = gp.GeoDataFrame(df, geometry=geometry)
            return gdf
        else:
            return df


    if __name__ == '__main__':
        ds = ogr.Open(sys.argv[1])
        lyr = ds.GetLayer(0)
        print(layer_as_geopandas(lyr))


Voting history
--------------

+1 from PSC members MateuszL, JukkaR, HowardB and EvenR
