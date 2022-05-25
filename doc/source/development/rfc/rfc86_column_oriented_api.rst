.. _rfc-86:

=============================================================
RFC 86: Column-oriented API for vector layers
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2022-May-24
Status:        Development
Target:        GDAL 3.6
============== =============================================

Summary
-------

This RFC describes the addition of new methods to the :cpp:class:`OGRLayer` class to retrieve
batches of features with a column oriented memory layout, that suits formats that
have that organization or downstream consumers that expect data to be presented
in such a way, in particular the `Apache Arrow <https://arrow.apache.org/docs/>`_,
`Pandas <https://pandas.pydata.org/>`_ / `GeoPandas <https://geopandas.org/>`_
ecosystem.

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
Similarly the above mentionned frameworks (Arrow, Pandas/GeoPandas) require
such memory layouts, and currently require reorganizing data when reading from OGR.
The `pyogrio <https://github.com/geopandas/pyogrio>`_ project is for example
an attempt at addressing that need.

Furthermore, the :ref:`vector.arrow` and :ref:`vector.parquet` drivers,
whose file organization is columnar, and batch oriented, have been added in GDAL 3.5.0.
Consequentl,y a columnar-oriented API will allow to get the best performance
from those formats.

Details
-------

The new proposed API uses and implements the
`Apache Arrow C data interface <https://arrow.apache.org/docs/format/CDataInterface.html>`_.
Reading the first paragraphs of that document (details on the various data types
can be skipped) is strongly encouraged for a better understanding of the rest of this RFC.

This interface is a set of 2 C structures:

- ArrowSchema: describe a set of fields (name, type, metadata). The list of Arrow
  data types and cover well OGR data types.

- ArrowArray: captures a set of values for a specific column/field in a subset
  of features. This is the equivalent of a
  `Series <https://arrow.apache.org/docs/python/pandas.html#series>`_ in a Pandas DataFrame.
  This is a potentially hiearchical structure that can aggregate
  sub arrays, and in OGR usage, the main array will be a StructArray which is
  the collection of OGR attribute and geometry fields.
  The layout of buffers and children arrays per data type is detailed in the
  `Arrow Columnar Format <https://arrow.apache.org/docs/format/Columnar.html>`_.

If a layer consists of 4 features of 2 fields (one of integer type, one of
floating-point type), the representation as a ArrowArray is *conceptually* the
following one:

.. code-block:: c

    array.children[0].buffers[1] = { 1, 2, 3, 4 };
    array.children[1].buffers[1] = { 1.2, 2.3, 3.4, 4.5 };

The content of a whole layer can be seen as a sequence of record batches, each
record batches being an ArrowArray of a subset of features. Instead of iterating
over individual features, one iterates over a batch/subset of several features at
once.

The ArrowSchema and ArrowArray structures are defined in a ogr_recordbatch.h
public header file, directly derived from https://github.com/apache/arrow/blob/master/cpp/src/arrow/c/abi.h
to get API/ABI compatibility with Apache Arrow C++. This header file must be
explicitly included when the related array batch API is used.

The following virtual methods are added to the OGRLayer class:

- GetRecordBatchSchema(): retrieve the layer definition as a ArrowSchema,
  consistent with the record batches returned by GetNextRecordBatch().

  .. code-block:: cpp

        virtual bool  GetRecordBatchSchema(struct ArrowSchema* out_schema,
                                           CSLConstList papszOptions = nullptr);

  out_schema is a pointer to a ArrowSchema structure, that can be in a uninitialized
  state (the method will ignore any initial content).

  OGRLayer has a base implementation for this method that maps all the FID
  column, all OGR attribute fields and geometry fields to Arrow fields. Specialized
  implementations may override this method (if doing that, GetNextRecordBatch()
  should also generally be overridden, so that returns of both methods are
  consistent.)

  The top-level schema object returned is of type Struct, whose children are the
  individual fields.

  When this method returns true, and the schema is no longer needed, it must
  be released with the following procedure, to take into account that it might
  have been released by other code, as documented in the Arrow C data
  interface:

  .. code-block:: c

          if( out_schema->release )
              out_schema->release(out_schema)


- GetNextRecordBatch(): retrieve the next record batch over the layer.

  .. code-block:: cpp

        virtual bool  GetNextRecordBatch(struct ArrowArray* out_array,
                                         struct ArrowSchema* out_schema = nullptr,
                                         CSLConstList papszOptions = nullptr);

  out_array is a pointer to a ArrowArray structure, that can be in a uninitialized
  state (the method will ignore any initial content).

  The default implementation uses GetNextFeature() internally to retrieve batches
  of up to 65,536 features. The starting address of buffers allocated by the
  default implementation is aligned on 64-byte boundaries.

  The default implementation outputs geometries as WKB in a binary field,
  whose corresponding entry in the schema is marked with the metadata item
  ``ARROW:extension:name`` set to ``WKB``. Specialized implementation may output
  by default other formats (particularly the Arrow driver that can return geometries
  encoded according to the GeoArrow specification (using list of coordinates).
  The GEOMETRY_ENCODING=WKB option can be passed to force the use of WKB (through
  the default implementation)

  The method may take into account ignored fields set with SetIgnoredFields() (the
  default implementation does), and should take into account filters set with
  SetSpatialFilter() and SetAttributeFilter(). Note however that specialized implementations
  may fallback to the default (slower) implementation when filters are set.

  out_schema is either NULL, or a pointer to a ArrowSchema structure, that can
  be in a uninitialized state (the method will ignore any initial content).

  The iteration might be reset with ResetReading(). Mixing calls to GetNextFeature()
  and GetNextRecordBatch() is not recommended, as the behaviour will be unspecified
  (but it should not crash).

  When this method returns true, and the array is no longer needed, it must
  be released with the following procedure, to take into account that it might
  have been released by other code, as documented in the Arrow C data
  interface:

  .. code-block:: c

          if( out_array->release )
              out_array->release(out_array)

  The out_schema, if non null, should be freed similarly, as documented in
  GetRecordBatchSchema().

The corresponding C functions, OGR_L_GetRecordBatchSchema and OGR_L_GetNextRecordBatch,
are added.

Other remarks
-------------

Using directly (as a producer or a consumer), the ArrowArray is admitedly not
trivial, and requires good intimacy with the Arrow C data interface specification,
to know, in which buffer of an array, data is to be read, which data type void*
buffers should be cast to, how to use buffers that contain null/not_null information,
how to use offset buffers for data types of type List, etc.
For the consuming side, the new API will be best used with the (Py)Arrow, Pandas,
GeoPandas, Numpy libraries which offer easier and safer access to record batches.
The study of the gdal_array._RecordBatchAsNumpy() method added to the SWIG Python
bindings can give a good hint of how to use an ArrowArray object, in conjunction
with the associated ArrowSchema.

It is not expected that most drivers will have a dedicated implementation of
GetNextRecordBatch(). Implementing it requires a non-trivial effort, and
significant gains are to be expected only for those for which I/O is very fast,
and thus in-memory shuffling of data takes a substantial time relatively to the
total time (I/O + shuffling).

Potential future work, no in the scope of this RFC, could be the addition of a
column-oriented method to write new features, a WriteRecordBatch() method.

Impacted drivers
----------------

- Arrow and Parquet: GetRecordBatchSchema() and GetNextRecordBatch() have a
  specialized implementation in those drivers that directly map to methods of
  the arrow-cpp library that bridges at near   zero cost (no data copying) the
  internal C++ implementation with the C data interface.

- FlatGeoBuf: a specialized implementation of GetNextRecordBatch() has been done,
  which saves going through the OGRFeature abstraction. See below benchmarks for
  measurement of the efficiency.

Bindings
--------

Per this RFC, only the Python bindings are extended to map the new functionality.

The ogr.Layer class receives the following new methods:

- GetRecordBatchSchemaAsPyArrow(): wrapper over OGRLayer::GetRecordBatchSchema() that
  turns the C ArrowSchema into a corresponding PyArrow Schema object

- GetNextRecordBatchAsPyArrow(): wrapper over OGRLayer::GetNextRecordBatch() that
  turns the C ArrowArray into a corresponding PyArrow Array object. This is a almost
  zero-cost call.

- RecordBatchesAsPyArrow(): return an iterator for GetNextRecordBatchAsPyArrow()

- GetNextRecordBatchAsNumpy(): wrapper over OGRLayer::GetNextRecordBatch() that
  turns the C ArrowArray into a Python dictionary whose keys are field names and
  values a Numpy array representing the values of the ArrowArray. The mapping of
  types is done for all Arrow data types returned by the base implementation of
  OGRLayer::GetNextRecordBatch(), but may not cover "exotic" data types that can
  be returned by specialized implementations such as the one in the Arrow/Parquet
  driver. For numeric data types, the Numpy array is a zero-copy adaptation of the
  C buffer. For other data types, a copy is involved, with potentially arrays of
  Python objects.

- RecordBatchesAsNumpy(): return an iterator for GetNextRecordBatchAsNumpy()


.. _rfc-86-benchmarks:

Benchmarks
----------

The test programs referenced in :ref:`rfc-86-annexes` have been run on a
dataset with 3.3 millions features, with 13 fields each (2 fields of type Integer,
8 of type String, 3 of type DateTime) and polygon geometries.

:ref:`rfc-86-bench-ogr-py`, :ref:`rfc-86-bench-fiona` and :ref:`rfc-86-bench-ogr-cpp`
have similar functionality: iterating over features with GetNextFeature().

:ref:`rfc-86-bench-pyogrio`, :ref:`rfc-86-bench-geopandas` and :ref:`rfc-86-bench-ogr-to-geopandas`
have similar functionality: building a GeoPandas GeoDataFrame

:ref:`rfc-86-bench-ogr-batch-cpp` can be used to measure the raw performance of the
proposed GetNextRecordBatch() API.

1. nz-building-outlines.fgb (FlatGeoBuf, 1.8 GB)

========================================  ============
        Bench program                      Timing (s)
========================================  ============
bench_ogr.cpp                             6.6
bench_ogr.py                              71
bench_fiona.py                            68
bench_pyogrio.py                          108
bench_geopandas.py                        232
bench_ogr_batch.cpp (driver impl.)        4.5
bench_ogr_batch.cpp (base impl.)          14.5
bench_ogr_to_geopandas.py (driver impl.)  11
bench_ogr_to_geopandas.py (base impl.)    20
========================================  ============

"driver impl." means that the specialized implementation of GetNextRecordBatch()
is used.
"base impl." means that the generic implementation of GetNextRecordBatch(),
using GetNextFeature() underneath, is used.

2. nz-building-outlines.parquet (GeoParquet, 436 MB)

========================================  ============
        Bench program                      Timing (s)
========================================  ============
bench_ogr.cpp                             6.4
bench_ogr.py                              72
bench_fiona.py                            70
bench_pyogrio.py                          115
bench_geopandas.py                        228
bench_ogr_batch.cpp (driver impl.)        1.6
bench_ogr_batch.cpp (base impl.)          14.1
bench_ogr_to_geopandas.py (driver impl.)  3.7
bench_ogr_to_geopandas.py (base impl.)    20.5
========================================  ============

Note: Fiona slightly modified to accept Parquet driver as a recognized one.

3. nz-building-outlines.gpkg (GeoPackage, 1.7 GB)

========================================  ============
        Bench program                      Timing (s)
========================================  ============
bench_ogr.cpp                             17.7
bench_ogr.py                              81
bench_fiona.py                            81
bench_pyogrio.py                          120
bench_geopandas.py                        258
bench_ogr_batch.cpp (driver impl.)        N/A
bench_ogr_batch.cpp (base impl.)          25
bench_ogr_to_geopandas.py (driver impl.)  N/A
bench_ogr_to_geopandas.py (base impl.)    33
========================================  ============

This demonstrates that:

- the new API can yield signficant performance gains to
  ingest a OGR layer as a GeoPandas GeoDataFrame, of the order of a 4x - 10x
  speed-up compared to pyogrio, even without a specialized implementation of
  GetNextRecordBatch(), and with formats that have a natural row organization (FlatGeoBuf).

- the Parquet driver is where this shines most due to the file organization being
  columnar, and its native access layer being ArrowArray compatible.

- for drivers that don't have a specialized implementation of GetNextRecordBatch()
  and whose layout is row oriented (GeoPackage), the GetNextFeature() approach is
  (a bit) faster than GetNextRecordBatch().

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
  by the methods GetRecordBatchSchemaAsPyArrow(), GetNextRecordBatchAsPyArrow(),
  record_batches_as_pyarrow().
  The GetNextRecordBatchAsNumpy() method is implemented internaly by the
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

.. _rfc-86-bench-pyogrio:

bench_pyogrio.py
++++++++++++++++

Use of the pyogrio Python library which uses the OGR C GetNextFeature() underneath to
expose a layer as GeoPandas GeoDataFrame.

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
        while( true )
        {
            struct ArrowArray array;
            if( !poLayer->GetNextRecordBatch(&array, nullptr, nullptr) )
                break;
            array.release(&array);
        }
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
        schema = lyr.GetRecordBatchSchemaAsPyArrow()

        geom_field_name = None
        for field in schema:
            field_md = field.metadata
            if field_md and field_md.get(b'ARROW:extension:name', None) == b'WKB':
                geom_field_name = field.name
                break

        fields = [field for field in schema]
        schema_without_geom = pa.schema(list(filter(lambda f: f.name != geom_field_name, fields)))
        batches_without_geom = []
        non_geom_field_names = [f.name for f in filter(lambda f: f.name != geom_field_name, fields)]
        if geom_field_name:
            schema_geom = pa.schema(list(filter(lambda f: f.name == geom_field_name, fields)))
            batches_with_geom = []
        for record_batch in lyr.RecordBatchesAsPyArrow():
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

TBD
