.. _csharp_vector:

================================================================================
C# Vector and Spatial Reference Interfaces
================================================================================

Basic Architecture
------------------

The vector interface is within the :file:`OSGeo.OGR` namespace and the spatial reference interface is within the :file:`OSGeo.OSR` namespace.

The **main** classes are as follows

::

    OGR
    |
    |- DataSource
    |
    |- Layer
    |
    |- Feature
    |
    |- Geometry

::

    OSR
    |
    |- SpatialReference
    |
    |- CoordinateTransform

Accessing Feature Geometries
----------------------------

The basic process is :file:`DataSource` => :file:`Layer` => :file:`Feature` => :file:`Geometry`

Open a DataSource
+++++++++++++++++

A :file:`DataSource` wraps a OGR source (e.g a filename) and is created as follows:

.. code-block:: 

    /* -------------------------------------------------------------------- */
    /*      Register format(s).                                             */
    /* -------------------------------------------------------------------- */
    Ogr.RegisterAll();

    /* -------------------------------------------------------------------- */
    /*      Open data source.                                               */
    /* -------------------------------------------------------------------- */
    using (DataSource ds = Ogr.Open( "... add your own valid OGR source", 0 ))
    {
        if (ds == null) {
            // create an error 
        }
        // Do your processing here
    }

Access the Layers
+++++++++++++++++

Each :file:`DataSource` will have one or more layers that can be iterated as follows:

.. code-block:: C#

    /* -------------------------------------------------------------------- */
    /*      Iterating through the layers                                    */
    /* -------------------------------------------------------------------- */

    for( int iLayer = 0; iLayer < ds.GetLayerCount(); iLayer++ )
    {
        Layer layer = ds.GetLayerByIndex(iLayer);

        if( layer == null )
        {
            // create an error 
        }
        // Do your processing here
    }

Access a Layer's Features
+++++++++++++++++++++++++

Each :file:`Layer` can have zero or more :file:`Feature` s. These should be accessed as follows:

.. code-block:: C#

    layer.ResetReading();
    Feature f = null;
    do {
        f = layer.GetNextFeature();
        if (f != null)
            // Do your processing here
    } while (f != null);

Access a Features's Geometry
++++++++++++++++++++++++++++

.. code-block:: C#

    Geometry geom = feature.GetGeometryRef();
    wkbGeometryType type = geom.GetGeometryType();

:file:`Geometry` objects are nested - so for instance:

* a :file:`Geometry` of type :file:`wkbGeometryType.wkbTIN` has multiple daughter :file:`Geometry` objects of type :file:`wkbGeometryType.wkbTriangle`,
* each :file:`Geometry` object of type :file:`wkbGeometryType.wkbTriangle` has a daughter :file:`Geometry` object of type :file:`wkbGeometryType.LinearRing`,
* each :file:`Geometry` of type :file:`wkbGeometryType.LinearRing` contains a number of points.

When you get to the most basic type - which usually seems to be :file:`wkbGeometryType.wkbPoint`, :file:`wkbGeometryType.wkbLineString` or :file:`wkbGeometryType.wkbLinearRing` or their multi- versions or 25D or ZM versions, you can
access the point coordinates as follows:

.. code-block:: C#

    int count = geom.GetPointCount();
    if (count > 0)
        for (int i = 0; i < count; i++) {
            double[] argout = new double[3];
            geom.GetPoint(i, argout);
            // do your processing here
        }

.. note:: The size of the :file:`double[]` depends on the number of dimensions of the :file:`Geometry`.

Access a Feature's data fields
++++++++++++++++++++++++++++++

Each :file:`Feature` object can have a number of data fields associated. The schema for the data fields 
is defined in a :file:`FieldDefn` object. The fields can be fetched a follows:

.. code-block:: C#

    Dictionary<string, object> ret = new Dictionary<string, object>();
    if (feature != null) {
        int fieldCount = feature.GetFieldCount();
        for (int i = 0; i < fieldCount; i++) {
            FieldDefn fd = feature.GetFieldDefnRef(i);
            string key = fd.GetName();
            object value = null;
            FieldType ft = fd.GetFieldType();
            switch (ft) {
                case FieldType.OFTString:
                    value = feature.GetFieldAsString(i);
                    break;
                case FieldType.OFTReal:
                    value = feature.GetFieldAsDouble(i);
                    break;
                case FieldType.OFTInteger:
                    value = feature.GetFieldAsInteger(i);
                    break;
                // Note this is only a sub-set of the possible field types
            }
            ret.Add(key, value);
        }
    }


Access a Geometry's CRS
+++++++++++++++++++++++

If there is a CRS (aka SRS) defined for the :file:`Geometry` it can be retrieved as follows:

.. code-block:: C#

    SpatialReference crs = geom.GetSpatialReference()

The :file:`SpatialReference` is the main class for representing the CRS / projection.
The CRS can be turned into a WKT string, e.g. for display purposes, as follows:

.. code-block:: C#

    string wkt;
    crs.ExportToWkt(out wkt, null);

.. note:: Sometimes the CRS defined on the layer does not cascade down to the Feature - you need to refer bak to the Layer


Reproject a Geometry
++++++++++++++++++++

If the :file:`Geometry` has a valid :file:`SpatialReference` defined, then the :file:`Geometry`
can be transformed to a new CRS using this command:

.. code-block:: C#

    if (geom.TransformTo(newProjection) != 0)
        throw new NotSupportedException("projection failed");

However, often it is better to explicitly define the :file:`CoordinateTransform` to be used


.. code-block:: C#

    SpatialReference from_crs = new SpatialReference(null) 
        // note - if you are defining from wkt - replace the null with the wkt
    from_crs.SetWellKnownGeogCS("EPSG:4326");
    
    SpatialReference to_crs = new SpatialReference(null);
    to_crs.ImportFromEPSG(27700);
    
    CoordinateTransform ct = new CoordinateTransform(from_crs, to_crs, new CoordinateTransformationOptions())
        // You can use the CoordinateTransformationOptions to set the operation or area of interet etc
    
    if (geom.Transform(ct) != 0)
        throw new NotSupportedException("projection failed");


Related C# examples
+++++++++++++++++++

The following examples demonstrate the usage of the OGR vector operations mentioned above:

* `ogrinfo.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/ogrinfo.cs>`__
* `OGRLayerAlg.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/OGRLayerAlg.cs>`__
* `OGRFeatureEdit.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/OGRFeatureEdit.cs>`__
* `OSRTransform.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/OSRTransform.cs>`__
* `GetCRSInfo.cs <https://github.com/OSGeo/gdal/blob/master/gdal/swig/csharp/apps/GetCRSInfo.cs>`__
