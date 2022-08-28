.. _vector_api_tut:

================================================================================
Vector API tutorial
================================================================================

This document is intended to document using the OGR C++ classes to read
and write data from a file.  It is strongly advised that the reader first
review the :ref:`vector_data_model` document describing
the key classes and their roles in OGR.

It also includes code snippets for the corresponding functions in C and Python.

Reading From OGR
----------------

For purposes of demonstrating reading with OGR, we will construct a small
utility for dumping point layers from an OGR data source to stdout in
comma-delimited format.

Initially it is necessary to register all the format drivers that are desired.
This is normally accomplished by calling :cpp:func:`GDALAllRegister` which registers
all format drivers built into GDAL/OGR.

In C++ :

.. code-block:: c++

    #include "ogrsf_frmts.h"

    int main()

    {
        GDALAllRegister();


In C :

.. code-block:: c

    #include "gdal.h"

    int main()

    {
        GDALAllRegister();

Next we need to open the input OGR datasource.  Datasources can be files,
RDBMSes, directories full of files, or even remote web services depending on
the driver being used.  However, the datasource name is always a single
string.  In this case we are hardcoded to open a particular shapefile.
The second argument (GDAL_OF_VECTOR) tells the :cpp:func:`OGROpen` method
that we want a vector driver to be use and that don't require update access.
On failure NULL is returned, and
we report an error.

In C++ :

.. code-block:: c++

    GDALDataset       *poDS;

    poDS = (GDALDataset*) GDALOpenEx( "point.shp", GDAL_OF_VECTOR, NULL, NULL, NULL );
    if( poDS == NULL )
    {
        printf( "Open failed.\n" );
        exit( 1 );
    }

In C :

.. code-block:: c

    GDALDatasetH hDS;

    hDS = GDALOpenEx( "point.shp", GDAL_OF_VECTOR, NULL, NULL, NULL );
    if( hDS == NULL )
    {
        printf( "Open failed.\n" );
        exit( 1 );
    }

A GDALDataset can potentially have many layers associated with it.  The
number of layers available can be queried with :cpp:func:`GDALDataset::GetLayerCount`
and individual layers fetched by index using :cpp:func:`GDALDataset::GetLayer`.
However, we will just fetch the layer by name.

In C++ :

.. code-block:: c++

    OGRLayer  *poLayer;

    poLayer = poDS->GetLayerByName( "point" );

In C :

.. code-block:: c

    OGRLayerH hLayer;

    hLayer = GDALDatasetGetLayerByName( hDS, "point" );


Now we want to start reading features from the layer.  Before we start we
could assign an attribute or spatial filter to the layer to restrict the set
of feature we get back, but for now we are interested in getting all features.

With GDAL 2.3 and C++11:

.. code-block:: c++

    for( auto& poFeature: poLayer )
    {

With GDAL 2.3 and C:

.. code-block:: c

    OGR_FOR_EACH_FEATURE_BEGIN(hFeature, hLayer)
    {

If using older GDAL versions, while it isn't strictly necessary in this
circumstance since we are starting fresh with the layer, it is often wise
to call :cpp:func:`OGRLayer::ResetReading` to ensure we are starting at the beginning of
the layer.  We iterate through all the features in the layer using
OGRLayer::GetNextFeature().  It will return NULL when we run out of features.

With GDAL < 2.3 and C++ :

.. code-block:: c++

    OGRFeature *poFeature;

    poLayer->ResetReading();
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {


With GDAL < 2.3 and C :

.. code-block:: c

    OGRFeatureH hFeature;

    OGR_L_ResetReading(hLayer);
    while( (hFeature = OGR_L_GetNextFeature(hLayer)) != NULL )
    {

In order to dump all the attribute fields of the feature, it is helpful
to get the :cpp:class:`OGRFeatureDefn`.  This is an object, associated with the layer,
containing the definitions of all the fields.  We loop over all the fields,
and fetch and report the attributes based on their type.

With GDAL 2.3 and C++11:

.. code-block:: c++

    for( auto&& oField: *poFeature )
    {
        switch( oField.GetType() )
        {
            case OFTInteger:
                printf( "%d,", oField.GetInteger() );
                break;
            case OFTInteger64:
                printf( CPL_FRMT_GIB ",", oField.GetInteger64() );
                break;
            case OFTReal:
                printf( "%.3f,", oField.GetDouble() );
                break;
            case OFTString:
                printf( "%s,", oField.GetString() );
                break;
            default:
                printf( "%s,", oField.GetAsString() );
                break;
        }
    }

With GDAL < 2.3 and C++ :

.. code-block:: c

    OGRFeatureDefn *poFDefn = poLayer->GetLayerDefn();
    for( int iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn( iField );

        switch( poFieldDefn->GetType() )
        {
            case OFTInteger:
                printf( "%d,", poFeature->GetFieldAsInteger( iField ) );
                break;
            case OFTInteger64:
                printf( CPL_FRMT_GIB ",", poFeature->GetFieldAsInteger64( iField ) );
                break;
            case OFTReal:
                printf( "%.3f,", poFeature->GetFieldAsDouble(iField) );
                break;
            case OFTString:
                printf( "%s,", poFeature->GetFieldAsString(iField) );
                break;
            default:
                printf( "%s,", poFeature->GetFieldAsString(iField) );
                break;
        }
    }

In C :

.. code-block:: c

    OGRFeatureDefnH hFDefn = OGR_L_GetLayerDefn(hLayer);
    int iField;

    for( iField = 0; iField < OGR_FD_GetFieldCount(hFDefn); iField++ )
    {
        OGRFieldDefnH hFieldDefn = OGR_FD_GetFieldDefn( hFDefn, iField );

        switch( OGR_Fld_GetType(hFieldDefn) )
        {
            case OFTInteger:
                printf( "%d,", OGR_F_GetFieldAsInteger( hFeature, iField ) );
                break;
            case OFTInteger64:
                printf( CPL_FRMT_GIB ",", OGR_F_GetFieldAsInteger64( hFeature, iField ) );
                break;
            case OFTReal:
                printf( "%.3f,", OGR_F_GetFieldAsDouble( hFeature, iField) );
                break;
            case OFTString:
                printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                break;
            default:
                printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                break;
        }
    }

There are a few more field types than those explicitly handled above, but
a reasonable representation of them can be fetched with the
:cpp:func:`OGRFeature::GetFieldAsString` method.  In fact we could shorten the above
by using GetFieldAsString() for all the types.

Next we want to extract the geometry from the feature, and write out the point
geometry x and y.   Geometries are returned as a generic :cpp:class:`OGRGeometry` pointer.
We then determine the specific geometry type, and if it is a point, we
cast it to point and operate on it.  If it is something else we write
placeholders.

In C++ :

.. code-block:: c++

    OGRGeometry *poGeometry;

    poGeometry = poFeature->GetGeometryRef();
    if( poGeometry != NULL
            && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
    {
    #if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(2,3,0)
        OGRPoint *poPoint = poGeometry->toPoint();
    #else
        OGRPoint *poPoint = (OGRPoint *) poGeometry;
    #endif

        printf( "%.3f,%3.f\n", poPoint->getX(), poPoint->getY() );
    }
    else
    {
        printf( "no point geometry\n" );
    }

In C :

.. code-block:: c

    OGRGeometryH hGeometry;

    hGeometry = OGR_F_GetGeometryRef(hFeature);
    if( hGeometry != NULL
            && wkbFlatten(OGR_G_GetGeometryType(hGeometry)) == wkbPoint )
    {
        printf( "%.3f,%3.f\n", OGR_G_GetX(hGeometry, 0), OGR_G_GetY(hGeometry, 0) );
    }
    else
    {
        printf( "no point geometry\n" );
    }

The :cpp:func:`wkbFlatten` macro is used above to convert the type for a wkbPoint25D
(a point with a z coordinate) into the base 2D geometry type code (wkbPoint).
For each 2D geometry type there is a corresponding 2.5D type code.  The 2D
and 2.5D geometry cases are handled by the same C++ class, so our code will
handle 2D or 3D cases properly.

Several geometry fields can be associated to a feature.

In C++ :

.. code-block:: c++

    OGRGeometry *poGeometry;
    int iGeomField;
    int nGeomFieldCount;

    nGeomFieldCount = poFeature->GetGeomFieldCount();
    for(iGeomField = 0; iGeomField < nGeomFieldCount; iGeomField ++ )
    {
        poGeometry = poFeature->GetGeomFieldRef(iGeomField);
        if( poGeometry != NULL
                && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
        {
    #if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(2,3,0)
            OGRPoint *poPoint = poGeometry->toPoint();
    #else
            OGRPoint *poPoint = (OGRPoint *) poGeometry;
    #endif

            printf( "%.3f,%3.f\n", poPoint->getX(), poPoint->getY() );
        }
        else
        {
            printf( "no point geometry\n" );
        }
    }


In C :

.. code-block:: c

    OGRGeometryH hGeometry;
    int iGeomField;
    int nGeomFieldCount;

    nGeomFieldCount = OGR_F_GetGeomFieldCount(hFeature);
    for(iGeomField = 0; iGeomField < nGeomFieldCount; iGeomField ++ )
    {
        hGeometry = OGR_F_GetGeomFieldRef(hFeature, iGeomField);
        if( hGeometry != NULL
                && wkbFlatten(OGR_G_GetGeometryType(hGeometry)) == wkbPoint )
        {
            printf( "%.3f,%3.f\n", OGR_G_GetX(hGeometry, 0),
                    OGR_G_GetY(hGeometry, 0) );
        }
        else
        {
            printf( "no point geometry\n" );
        }
    }


In Python:

.. code-block:: python

    nGeomFieldCount = feat.GetGeomFieldCount()
    for iGeomField in range(nGeomFieldCount):
        geom = feat.GetGeomFieldRef(iGeomField)
        if geom is not None and geom.GetGeometryType() == ogr.wkbPoint:
            print "%.3f, %.3f" % ( geom.GetX(), geom.GetY() )
        else:
            print "no point geometry\n"

Note that :cpp:func:`OGRFeature::GetGeometryRef` and :cpp:func:`OGRFeature::GetGeomFieldRef`
return a pointer to
the internal geometry owned by the OGRFeature.  There we don't actually
delete the return geometry.


With GDAL 2.3 and C++11, the looping over features is simply terminated by
a closing curly bracket.

.. code-block:: c++

    }

With GDAL 2.3 and C, the looping over features is simply terminated by
the following.

.. code-block:: c

    }
    OGR_FOR_EACH_FEATURE_END(hFeature)


For GDAL < 2.3, as the :cpp:func:`OGRLayer::GetNextFeature` method
returns a copy of the feature that is now owned by us.  So at the end of
use we must free the feature.  We could just "delete" it, but this can cause
problems in windows builds where the GDAL DLL has a different "heap" from the
main program.  To be on the safe side we use a GDAL function to delete the
feature.

In C++ :

.. code-block:: c++

        OGRFeature::DestroyFeature( poFeature );
    }

In C :

.. code-block:: c

        OGR_F_Destroy( hFeature );
    }


The OGRLayer returned by :cpp:func:`GDALDataset::GetLayerByName` is also a reference
to an internal layer owned by the GDALDataset so we don't need to delete
it.  But we do need to delete the datasource in order to close the input file.
Once again we do this with a custom delete method to avoid special win32
heap issues.

In C/C++ :

.. code-block:: c++

        GDALClose( poDS );
    }


All together our program looks like this.

With GDAL 2.3 and C++11 :

.. code-block:: c++

    #include "ogrsf_frmts.h"

    int main()

    {
        GDALAllRegister();

        GDALDatasetUniquePtr poDS(GDALDataset::Open( "point.shp", GDAL_OF_VECTOR));
        if( poDS == nullptr )
        {
            printf( "Open failed.\n" );
            exit( 1 );
        }

        for( const OGRLayer* poLayer: poDS->GetLayers() )
        {
            for( const auto& poFeature: *poLayer )
            {
                for( const auto& oField: *poFeature )
                {
                    switch( oField.GetType() )
                    {
                        case OFTInteger:
                            printf( "%d,", oField.GetInteger() );
                            break;
                        case OFTInteger64:
                            printf( CPL_FRMT_GIB ",", oField.GetInteger64() );
                            break;
                        case OFTReal:
                            printf( "%.3f,", oField.GetDouble() );
                            break;
                        case OFTString:
                            printf( "%s,", oField.GetString() );
                            break;
                        default:
                            printf( "%s,", oField.GetAsString() );
                            break;
                    }
                }

                const OGRGeometry *poGeometry = poFeature->GetGeometryRef();
                if( poGeometry != nullptr
                        && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
                {
                    const OGRPoint *poPoint = poGeometry->toPoint();

                    printf( "%.3f,%3.f\n", poPoint->getX(), poPoint->getY() );
                }
                else
                {
                    printf( "no point geometry\n" );
                }
            }
        }
        return 0;
    }

In C++ :

.. code-block:: c++

    #include "ogrsf_frmts.h"

    int main()

    {
        GDALAllRegister();

        GDALDataset *poDS = static_cast<GDALDataset*>(
            GDALOpenEx( "point.shp", GDAL_OF_VECTOR, NULL, NULL, NULL ));
        if( poDS == NULL )
        {
            printf( "Open failed.\n" );
            exit( 1 );
        }

        OGRLayer  *poLayer = poDS->GetLayerByName( "point" );
        OGRFeatureDefn *poFDefn = poLayer->GetLayerDefn();

        poLayer->ResetReading();
        OGRFeature *poFeature;
        while( (poFeature = poLayer->GetNextFeature()) != NULL )
        {
            for( int iField = 0; iField < poFDefn->GetFieldCount(); iField++ )
            {
                OGRFieldDefn *poFieldDefn = poFDefn->GetFieldDefn( iField );

                switch( poFieldDefn->GetType() )
                {
                    case OFTInteger:
                        printf( "%d,", poFeature->GetFieldAsInteger( iField ) );
                        break;
                    case OFTInteger64:
                        printf( CPL_FRMT_GIB ",", poFeature->GetFieldAsInteger64( iField ) );
                        break;
                    case OFTReal:
                        printf( "%.3f,", poFeature->GetFieldAsDouble(iField) );
                        break;
                    case OFTString:
                        printf( "%s,", poFeature->GetFieldAsString(iField) );
                        break;
                    default:
                        printf( "%s,", poFeature->GetFieldAsString(iField) );
                        break;
                }
            }

            OGRGeometry *poGeometry = poFeature->GetGeometryRef();
            if( poGeometry != NULL
                    && wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
            {
                OGRPoint *poPoint = (OGRPoint *) poGeometry;

                printf( "%.3f,%3.f\n", poPoint->getX(), poPoint->getY() );
            }
            else
            {
                printf( "no point geometry\n" );
            }
            OGRFeature::DestroyFeature( poFeature );
        }

        GDALClose( poDS );
    }

In C :

.. code-block:: c

    #include "gdal.h"

    int main()

    {
        GDALAllRegister();

        GDALDatasetH hDS;
        OGRLayerH hLayer;
        OGRFeatureH hFeature;
        OGRFeatureDefnH hFDefn;

        hDS = GDALOpenEx( "point.shp", GDAL_OF_VECTOR, NULL, NULL, NULL );
        if( hDS == NULL )
        {
            printf( "Open failed.\n" );
            exit( 1 );
        }

        hLayer = GDALDatasetGetLayerByName( hDS, "point" );
        hFDefn = OGR_L_GetLayerDefn(hLayer);

        OGR_L_ResetReading(hLayer);
        while( (hFeature = OGR_L_GetNextFeature(hLayer)) != NULL )
        {
            int iField;
            OGRGeometryH hGeometry;

            for( iField = 0; iField < OGR_FD_GetFieldCount(hFDefn); iField++ )
            {
                OGRFieldDefnH hFieldDefn = OGR_FD_GetFieldDefn( hFDefn, iField );

                switch( OGR_Fld_GetType(hFieldDefn) )
                {
                    case OFTInteger:
                        printf( "%d,", OGR_F_GetFieldAsInteger( hFeature, iField ) );
                        break;
                    case OFTInteger64:
                        printf( CPL_FRMT_GIB ",", OGR_F_GetFieldAsInteger64( hFeature, iField ) );
                        break;
                    case OFTReal:
                        printf( "%.3f,", OGR_F_GetFieldAsDouble( hFeature, iField) );
                        break;
                    case OFTString:
                        printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                        break;
                    default:
                        printf( "%s,", OGR_F_GetFieldAsString( hFeature, iField) );
                        break;
                }
            }

            hGeometry = OGR_F_GetGeometryRef(hFeature);
            if( hGeometry != NULL
                && wkbFlatten(OGR_G_GetGeometryType(hGeometry)) == wkbPoint )
            {
                printf( "%.3f,%3.f\n", OGR_G_GetX(hGeometry, 0), OGR_G_GetY(hGeometry, 0) );
            }
            else
            {
                printf( "no point geometry\n" );
            }

            OGR_F_Destroy( hFeature );
        }

        GDALClose( hDS );
    }


In Python:

.. code-block:: python

    import sys
    from osgeo import gdal

    ds = gdal.OpenEx( "point.shp", gdal.OF_VECTOR )
    if ds is None:
        print "Open failed.\n"
        sys.exit( 1 )

    lyr = ds.GetLayerByName( "point" )

    lyr.ResetReading()

    for feat in lyr:

        feat_defn = lyr.GetLayerDefn()
        for i in range(feat_defn.GetFieldCount()):
            field_defn = feat_defn.GetFieldDefn(i)

            # Tests below can be simplified with just :
            # print feat.GetField(i)
            if field_defn.GetType() == ogr.OFTInteger or field_defn.GetType() == ogr.OFTInteger64:
                print "%d" % feat.GetFieldAsInteger64(i)
            elif field_defn.GetType() == ogr.OFTReal:
                print "%.3f" % feat.GetFieldAsDouble(i)
            elif field_defn.GetType() == ogr.OFTString:
                print "%s" % feat.GetFieldAsString(i)
            else:
                print "%s" % feat.GetFieldAsString(i)

        geom = feat.GetGeometryRef()
        if geom is not None and geom.GetGeometryType() == ogr.wkbPoint:
            print "%.3f, %.3f" % ( geom.GetX(), geom.GetY() )
        else:
            print "no point geometry\n"

    ds = None

.. _vector_api_tut_arrow_stream:

Reading From OGR using the Arrow C Stream data interface
--------------------------------------------------------

.. versionadded:: 3.6

Instead of retrieving features one at a time, it is also possible to retrive
them by batches, with a column-oriented memory layout, using the
:cpp:func:`OGRLayer::GetArrowStream` method. Note that this method is more
difficult to use than the traditional :cpp:func:`OGRLayer::GetNextFeature` approach,
and is only advised when compatibility with the
`Apache Arrow C Stream interface <https://arrow.apache.org/docs/format/CStreamInterface.html>`_
is needed, or when column-oriented consumption of layers is required.

Pending using an helper library, consumption of the Arrow C Stream interface
requires reading of the following documents:

- `Arrow C Stream interface <https://arrow.apache.org/docs/format/CStreamInterface.html>`_
- `Arrow C data interface <https://arrow.apache.org/docs/format/CDataInterface.html>`_
- `Arrow Columnar Format <https://arrow.apache.org/docs/format/Columnar.html>`_.

The Arrow C Stream interface interface consists of a set of C structures, ArrowArrayStream, that provides
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

The ArrowArrayStream, ArrowSchema, ArrowArray structures are defined in a
ogr_recordbatch.h public header file, directly derived from
https://github.com/apache/arrow/blob/master/cpp/src/arrow/c/abi.h
to get API/ABI compatibility with Apache Arrow C++. This header file must be
explicitly included when the related array batch API is used.

The GetArrowStream() method has the followin signature:

  .. code-block:: cpp

        virtual bool OGRLayer::GetArrowStream(struct ArrowArrayStream* out_stream,
                                              CSLConstList papszOptions = nullptr);

It is also available in the C API as :cpp:func:`OGR_L_GetArrowStream`.

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

The papszOptions that may be provided is a NULL terminated list of key=value
strings, that may be driver specific.

OGRLayer has a base implementation of GetArrowStream() that is such:

- The get_schema() callback returns a schema whose top-level object returned is
  of type Struct, and whose children consist of the FID column, all OGR attribute
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

Drivers that have a specialized implementation advertize the
new OLCFastGetArrowStream layer capability.

Using directly (as a producer or a consumer) a ArrowArray is admitedly not
trivial, and requires good intimacy with the Arrow C data interface and columnar
array specifications, to know, in which buffer of an array, data is to be read,
which data type void* buffers should be cast to, how to use buffers that contain
null/not_null information, how to use offset buffers for data types of type List, etc.
The study of the gdal_array._RecordBatchAsNumpy() method of the SWIG Python
bindings (https://github.com/OSGeo/gdal/blob/master/swig/include/gdal_array.i)
can give a good hint of how to use an ArrowArray object, in conjunction
with the associated ArrowSchema.

The below example illustrates how to read the content of a layer that consists
of a integer field and a geometry field:


.. code-block:: c++

    #include "gdal_priv.h"
    #include "ogr_api.h"
    #include "ogrsf_frmts.h"
    #include "ogr_recordbatch.h"
    #include <cassert>

    int main(int argc, char* argv[])
    {
        GDALAllRegister();
        GDALDataset* poDS = GDALDataset::Open(argv[1]);
        if( poDS == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Open() failed\n");
            exit(1);
        }
        OGRLayer* poLayer = poDS->GetLayer(0);
        OGRLayerH hLayer = OGRLayer::ToHandle(poLayer);

        // Get the Arrow stream
        struct ArrowArrayStream stream;
        if( !OGR_L_GetArrowStream(hLayer, &stream, nullptr))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "OGR_L_GetArrowStream() failed\n");
            delete poDS;
            exit(1);
        }

        // Get the schema
        struct ArrowSchema schema;
        if( stream.get_schema(&stream, &schema) != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "get_schema() failed\n");
            stream.release(&stream);
            delete poDS;
            exit(1);
        }

        // Check that the returned schema consists of one int64 field (for FID),
        // one int32 field and one binary/wkb field
        if( schema.n_children != 3 ||
            strcmp(schema.children[0]->format, "l") != 0 || // int64 -> FID
            strcmp(schema.children[1]->format, "i") != 0 || // int32
            strcmp(schema.children[2]->format, "z") != 0 )  // binary for WKB
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Layer has not the expected schema required by this example.");
            schema.release(&schema);
            stream.release(&stream);
            delete poDS;
            exit(1);
        }
        schema.release(&schema);

        // Iterate over batches
        while( true )
        {
            struct ArrowArray array;
            if( stream.get_next(&stream, &array) != 0 ||
                array.release == nullptr )
            {
                break;
            }

            assert(array.n_children == 3);

            // Cast the array->children[].buffers[] to the appropriate data types
            const auto int_child = array.children[1];
            assert(int_child->n_buffers == 2);
            const uint8_t* int_field_not_null = static_cast<const uint8_t*>(int_child->buffers[0]);
            const int32_t* int_field = static_cast<const int32_t*>(int_child->buffers[1]);

            const auto wkb_child = array.children[2];
            assert(wkb_child->n_buffers == 3);
            const uint8_t* wkb_field_not_null = static_cast<const uint8_t*>(wkb_child->buffers[0]);
            const int32_t* wkb_offset = static_cast<const int32_t*>(wkb_child->buffers[1]);
            const uint8_t* wkb_field = static_cast<const uint8_t*>(wkb_child->buffers[2]);

            // Lambda to check if a field is set for a given feature index
            const auto IsSet = [](const uint8_t* buffer_not_null, int i)
            {
                return buffer_not_null == nullptr || (buffer_not_null[i/8] >> (i%8)) != 0;
            };

            // Iterate through features of a batch
            for( long long i = 0; i < array.length; i++ )
            {
                if( IsSet(int_field_not_null, i) )
                    printf("int_field[%lld] = %d\n", i, int_field[i]);
                else
                    printf("int_field[%lld] = null\n", i);

                if( IsSet(wkb_field_not_null, i) )
                {
                    const void* wkb = wkb_field + wkb_offset[i];
                    const int32_t length = wkb_offset[i+1] - wkb_offset[i];
                    char* wkt = nullptr;
                    OGRGeometry* geom = nullptr;
                    OGRGeometryFactory::createFromWkb(wkb, nullptr, &geom, length);
                    if( geom )
                    {
                        geom->exportToWkt(&wkt);
                    }
                    printf("wkb_field[%lld] = %s\n", i, wkt ? wkt : "invalid geometry");
                    CPLFree(wkt);
                    delete geom;
                }
                else
                {
                    printf("wkb_field[%lld] = null\n", i);
                }
            }

            // Release memory taken by the batch
            array.release(&array);
        }

        // Release stream and dataset
        stream.release(&stream);
        delete poDS;
        return 0;
    }


Writing To OGR
--------------

As an example of writing through OGR, we will do roughly the opposite of the
above.  A short program that reads comma separated values from input text
will be written to a point shapefile via OGR.

As usual, we start by registering all the drivers, and then fetch the
Shapefile driver as we will need it to create our output file.

In C++ :

.. code-block:: c++

    #include "ogrsf_frmts.h"

    int main()
    {
        const char *pszDriverName = "ESRI Shapefile";
        GDALDriver *poDriver;

        GDALAllRegister();

        poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
        if( poDriver == NULL )
        {
            printf( "%s driver not available.\n", pszDriverName );
            exit( 1 );
        }

In C :

.. code-block:: c

    #include "ogr_api.h"

    int main()
    {
        const char *pszDriverName = "ESRI Shapefile";
        GDALDriver *poDriver;

        GDALAllRegister();

        poDriver = (GDALDriver*) GDALGetDriverByName(pszDriverName );
        if( poDriver == NULL )
        {
            printf( "%s driver not available.\n", pszDriverName );
            exit( 1 );
        }

Next we create the datasource.  The ESRI Shapefile driver allows us to create
a directory full of shapefiles, or a single shapefile as a datasource.  In
this case we will explicitly create a single file by including the extension
in the name.  Other drivers behave differently.
The second, third, fourth and fifth argument are related to raster dimensions
(in case the driver has raster capabilities). The last argument to
the call is a list of option values, but we will just be using defaults in
this case.  Details of the options supported are also format specific.

In C ++ :

.. code-block:: c++

    GDALDataset *poDS;

    poDS = poDriver->Create( "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
    if( poDS == NULL )
    {
        printf( "Creation of output file failed.\n" );
        exit( 1 );
    }


In C :

.. code-block:: c

    GDALDatasetH hDS;

    hDS = GDALCreate( hDriver, "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
    if( hDS == NULL )
    {
        printf( "Creation of output file failed.\n" );
        exit( 1 );
    }

Now we create the output layer.  In this case since the datasource is a
single file, we can only have one layer.  We pass wkbPoint to specify the
type of geometry supported by this layer.  In this case we aren't passing
any coordinate system information or other special layer creation options.

In C++ :

.. code-block:: c++

    OGRLayer *poLayer;

    poLayer = poDS->CreateLayer( "point_out", NULL, wkbPoint, NULL );
    if( poLayer == NULL )
    {
        printf( "Layer creation failed.\n" );
        exit( 1 );
    }


In C :

.. code-block:: c

    OGRLayerH hLayer;

    hLayer = GDALDatasetCreateLayer( hDS, "point_out", NULL, wkbPoint, NULL );
    if( hLayer == NULL )
    {
        printf( "Layer creation failed.\n" );
        exit( 1 );
    }


Now that the layer exists, we need to create any attribute fields that should
appear on the layer.  Fields must be added to the layer before any features
are written.  To create a field we initialize an :cpp:union:`OGRField` object with the
information about the field.  In the case of Shapefiles, the field width and
precision is significant in the creation of the output .dbf file, so we
set it specifically, though generally the defaults are OK.  For this example
we will just have one attribute, a name string associated with the x,y point.

Note that the template OGRField we pass to :cpp:func:`OGRLayer::CreateField` is copied internally.
We retain ownership of the object.

In C++:

.. code-block:: c++

    OGRFieldDefn oField( "Name", OFTString );

    oField.SetWidth(32);

    if( poLayer->CreateField( &oField ) != OGRERR_NONE )
    {
        printf( "Creating Name field failed.\n" );
        exit( 1 );
    }


In C:

.. code-block:: c

    OGRFieldDefnH hFieldDefn;

    hFieldDefn = OGR_Fld_Create( "Name", OFTString );

    OGR_Fld_SetWidth( hFieldDefn, 32);

    if( OGR_L_CreateField( hLayer, hFieldDefn, TRUE ) != OGRERR_NONE )
    {
        printf( "Creating Name field failed.\n" );
        exit( 1 );
    }

    OGR_Fld_Destroy(hFieldDefn);


The following snipping loops reading lines of the form "x,y,name" from stdin,
and parsing them.

In C++ and in C :

.. code-block:: c

    double x, y;
    char szName[33];

    while( !feof(stdin)
           && fscanf( stdin, "%lf,%lf,%32s", &x, &y, szName ) == 3 )
    {

To write a feature to disk, we must create a local OGRFeature, set attributes
and attach geometry before trying to write it to the layer.  It is
imperative that this feature be instantiated from the OGRFeatureDefn
associated with the layer it will be written to.

In C++ :

.. code-block:: c++

        OGRFeature *poFeature;

        poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
        poFeature->SetField( "Name", szName );

In C :

.. code-block:: c

        OGRFeatureH hFeature;

        hFeature = OGR_F_Create( OGR_L_GetLayerDefn( hLayer ) );
        OGR_F_SetFieldString( hFeature, OGR_F_GetFieldIndex(hFeature, "Name"), szName );

We create a local geometry object, and assign its copy (indirectly) to the feature.
The :cpp:func:`OGRFeature::SetGeometryDirectly` differs from :cpp:func:`OGRFeature::SetGeometry`
in that the direct method gives ownership of the geometry to the feature.
This is generally more efficient as it avoids an extra deep object copy
of the geometry.

In C++ :

.. code-block:: c++

        OGRPoint pt;
        pt.setX( x );
        pt.setY( y );

        poFeature->SetGeometry( &pt );


In C :

.. code-block:: c

        OGRGeometryH hPt;
        hPt = OGR_G_CreateGeometry(wkbPoint);
        OGR_G_SetPoint_2D(hPt, 0, x, y);

        OGR_F_SetGeometry( hFeature, hPt );
        OGR_G_DestroyGeometry(hPt);


Now we create a feature in the file.  The :cpp:func:`OGRLayer::CreateFeature` does not
take ownership of our feature so we clean it up when done with it.

In C++ :

.. code-block:: c++

        if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
        {
            printf( "Failed to create feature in shapefile.\n" );
           exit( 1 );
        }

        OGRFeature::DestroyFeature( poFeature );
   }

In C :

.. code-block:: c

        if( OGR_L_CreateFeature( hLayer, hFeature ) != OGRERR_NONE )
        {
            printf( "Failed to create feature in shapefile.\n" );
           exit( 1 );
        }

        OGR_F_Destroy( hFeature );
   }


Finally we need to close down the datasource in order to ensure headers
are written out in an orderly way and all resources are recovered.

In C/C++ :

.. code-block:: c

        GDALClose( poDS );
    }


The same program all in one block looks like this:

In C++ :

.. code-block:: c++

    #include "ogrsf_frmts.h"

    int main()
    {
        const char *pszDriverName = "ESRI Shapefile";
        GDALDriver *poDriver;

        GDALAllRegister();

        poDriver = GetGDALDriverManager()->GetDriverByName(pszDriverName );
        if( poDriver == NULL )
        {
            printf( "%s driver not available.\n", pszDriverName );
            exit( 1 );
        }

        GDALDataset *poDS;

        poDS = poDriver->Create( "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
        if( poDS == NULL )
        {
            printf( "Creation of output file failed.\n" );
            exit( 1 );
        }

        OGRLayer *poLayer;

        poLayer = poDS->CreateLayer( "point_out", NULL, wkbPoint, NULL );
        if( poLayer == NULL )
        {
            printf( "Layer creation failed.\n" );
            exit( 1 );
        }

        OGRFieldDefn oField( "Name", OFTString );

        oField.SetWidth(32);

        if( poLayer->CreateField( &oField ) != OGRERR_NONE )
        {
            printf( "Creating Name field failed.\n" );
            exit( 1 );
        }

        double x, y;
        char szName[33];

        while( !feof(stdin)
            && fscanf( stdin, "%lf,%lf,%32s", &x, &y, szName ) == 3 )
        {
            OGRFeature *poFeature;

            poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );
            poFeature->SetField( "Name", szName );

            OGRPoint pt;

            pt.setX( x );
            pt.setY( y );

            poFeature->SetGeometry( &pt );

            if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
            {
                printf( "Failed to create feature in shapefile.\n" );
                exit( 1 );
            }

            OGRFeature::DestroyFeature( poFeature );
        }

        GDALClose( poDS );
    }


In C :

.. code-block:: c

    #include "gdal.h"

    int main()
    {
        const char *pszDriverName = "ESRI Shapefile";
        GDALDriverH hDriver;
        GDALDatasetH hDS;
        OGRLayerH hLayer;
        OGRFieldDefnH hFieldDefn;
        double x, y;
        char szName[33];

        GDALAllRegister();

        hDriver = GDALGetDriverByName( pszDriverName );
        if( hDriver == NULL )
        {
            printf( "%s driver not available.\n", pszDriverName );
            exit( 1 );
        }

        hDS = GDALCreate( hDriver, "point_out.shp", 0, 0, 0, GDT_Unknown, NULL );
        if( hDS == NULL )
        {
            printf( "Creation of output file failed.\n" );
            exit( 1 );
        }

        hLayer = GDALDatasetCreateLayer( hDS, "point_out", NULL, wkbPoint, NULL );
        if( hLayer == NULL )
        {
            printf( "Layer creation failed.\n" );
            exit( 1 );
        }

        hFieldDefn = OGR_Fld_Create( "Name", OFTString );

        OGR_Fld_SetWidth( hFieldDefn, 32);

        if( OGR_L_CreateField( hLayer, hFieldDefn, TRUE ) != OGRERR_NONE )
        {
            printf( "Creating Name field failed.\n" );
            exit( 1 );
        }

        OGR_Fld_Destroy(hFieldDefn);

        while( !feof(stdin)
            && fscanf( stdin, "%lf,%lf,%32s", &x, &y, szName ) == 3 )
        {
            OGRFeatureH hFeature;
            OGRGeometryH hPt;

            hFeature = OGR_F_Create( OGR_L_GetLayerDefn( hLayer ) );
            OGR_F_SetFieldString( hFeature, OGR_F_GetFieldIndex(hFeature, "Name"), szName );

            hPt = OGR_G_CreateGeometry(wkbPoint);
            OGR_G_SetPoint_2D(hPt, 0, x, y);

            OGR_F_SetGeometry( hFeature, hPt );
            OGR_G_DestroyGeometry(hPt);

            if( OGR_L_CreateFeature( hLayer, hFeature ) != OGRERR_NONE )
            {
            printf( "Failed to create feature in shapefile.\n" );
            exit( 1 );
            }

            OGR_F_Destroy( hFeature );
        }

        GDALClose( hDS );
    }


In Python :

.. code-block:: python

    import sys
    from osgeo import gdal
    from osgeo import ogr
    import string

    driverName = "ESRI Shapefile"
    drv = gdal.GetDriverByName( driverName )
    if drv is None:
        print "%s driver not available.\n" % driverName
        sys.exit( 1 )

    ds = drv.Create( "point_out.shp", 0, 0, 0, gdal.GDT_Unknown )
    if ds is None:
        print "Creation of output file failed.\n"
        sys.exit( 1 )

    lyr = ds.CreateLayer( "point_out", None, ogr.wkbPoint )
    if lyr is None:
        print "Layer creation failed.\n"
        sys.exit( 1 )

    field_defn = ogr.FieldDefn( "Name", ogr.OFTString )
    field_defn.SetWidth( 32 )

    if lyr.CreateField ( field_defn ) != 0:
        print "Creating Name field failed.\n"
        sys.exit( 1 )

    # Expected format of user input: x y name
    linestring = raw_input()
    linelist = string.split(linestring)

    while len(linelist) == 3:
        x = float(linelist[0])
        y = float(linelist[1])
        name = linelist[2]

        feat = ogr.Feature( lyr.GetLayerDefn())
        feat.SetField( "Name", name )

        pt = ogr.Geometry(ogr.wkbPoint)
        pt.SetPoint_2D(0, x, y)

        feat.SetGeometry(pt)

        if lyr.CreateFeature(feat) != 0:
            print "Failed to create feature in shapefile.\n"
            sys.exit( 1 )

        feat.Destroy()

        linestring = raw_input()
        linelist = string.split(linestring)

    ds = None


Several geometry fields can be associated to a feature. This capability
is just available for a few file formats, such as PostGIS.

To create such datasources, geometry fields must be first created.
Spatial reference system objects can be associated to each geometry field.

In C++ :

.. code-block:: c++

    OGRGeomFieldDefn oPointField( "PointField", wkbPoint );
    OGRSpatialReference* poSRS = new OGRSpatialReference();
    poSRS->importFromEPSG(4326);
    oPointField.SetSpatialRef(poSRS);
    poSRS->Release();

    if( poLayer->CreateGeomField( &oPointField ) != OGRERR_NONE )
    {
        printf( "Creating field PointField failed.\n" );
        exit( 1 );
    }

    OGRGeomFieldDefn oFieldPoint2( "PointField2", wkbPoint );
    poSRS = new OGRSpatialReference();
    poSRS->importFromEPSG(32631);
    oPointField2.SetSpatialRef(poSRS);
    poSRS->Release();

    if( poLayer->CreateGeomField( &oPointField2 ) != OGRERR_NONE )
    {
        printf( "Creating field PointField2 failed.\n" );
        exit( 1 );
    }


In C :

.. code-block:: c

    OGRGeomFieldDefnH hPointField;
    OGRGeomFieldDefnH hPointField2;
    OGRSpatialReferenceH hSRS;

    hPointField = OGR_GFld_Create( "PointField", wkbPoint );
    hSRS = OSRNewSpatialReference( NULL );
    OSRImportFromEPSG(hSRS, 4326);
    OGR_GFld_SetSpatialRef(hPointField, hSRS);
    OSRRelease(hSRS);

    if( OGR_L_CreateGeomField( hLayer, hPointField ) != OGRERR_NONE )
    {
        printf( "Creating field PointField failed.\n" );
        exit( 1 );
    }

    OGR_GFld_Destroy( hPointField );

    hPointField2 = OGR_GFld_Create( "PointField2", wkbPoint );
    OSRImportFromEPSG(hSRS, 32631);
    OGR_GFld_SetSpatialRef(hPointField2, hSRS);
    OSRRelease(hSRS);

    if( OGR_L_CreateGeomField( hLayer, hPointField2 ) != OGRERR_NONE )
    {
        printf( "Creating field PointField2 failed.\n" );
        exit( 1 );
    }

    OGR_GFld_Destroy( hPointField2 );


To write a feature to disk, we must create a local OGRFeature, set attributes
and attach geometries before trying to write it to the layer.  It is
imperative that this feature be instantiated from the OGRFeatureDefn
associated with the layer it will be written to.

In C++ :

.. code-block:: c++

        OGRFeature *poFeature;
        OGRGeometry *poGeometry;
        char* pszWKT;

        poFeature = OGRFeature::CreateFeature( poLayer->GetLayerDefn() );

        pszWKT = (char*) "POINT (2 49)";
        OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeometry );
        poFeature->SetGeomFieldDirectly( "PointField", poGeometry );

        pszWKT = (char*) "POINT (500000 4500000)";
        OGRGeometryFactory::createFromWkt( &pszWKT, NULL, &poGeometry );
        poFeature->SetGeomFieldDirectly( "PointField2", poGeometry );

        if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
        {
            printf( "Failed to create feature.\n" );
            exit( 1 );
        }

        OGRFeature::DestroyFeature( poFeature );

In C :

.. code-block:: c

        OGRFeatureH hFeature;
        OGRGeometryH hGeometry;
        char* pszWKT;

        poFeature = OGR_F_Create( OGR_L_GetLayerDefn(hLayer) );

        pszWKT = (char*) "POINT (2 49)";
        OGR_G_CreateFromWkt( &pszWKT, NULL, &hGeometry );
        OGR_F_SetGeomFieldDirectly( hFeature,
            OGR_F_GetGeomFieldIndex(hFeature, "PointField"), hGeometry );

        pszWKT = (char*) "POINT (500000 4500000)";
        OGR_G_CreateFromWkt( &pszWKT, NULL, &hGeometry );
        OGR_F_SetGeomFieldDirectly( hFeature,
            OGR_F_GetGeomFieldIndex(hFeature, "PointField2"), hGeometry );

        if( OGR_L_CreateFeature( hFeature ) != OGRERR_NONE )
        {
            printf( "Failed to create feature.\n" );
            exit( 1 );
        }

        OGR_F_Destroy( hFeature );


In Python :

.. code-block:: python

        feat = ogr.Feature( lyr.GetLayerDefn() )

        feat.SetGeomFieldDirectly( "PointField",
            ogr.CreateGeometryFromWkt( "POINT (2 49)" ) )
        feat.SetGeomFieldDirectly( "PointField2",
            ogr.CreateGeometryFromWkt( "POINT (500000 4500000)" ) )

        if lyr.CreateFeature( feat ) != 0:
            print( "Failed to create feature.\n" );
            sys.exit( 1 );
