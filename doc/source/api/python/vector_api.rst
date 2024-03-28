.. _python_vector_api:

Python Vector API
=================

This page contains classes, methods, functions that relate to the GDAL :ref:`vector_data_model`. The :py:class:`Driver` and :py:class:`Dataset` classes, which applies to both vector and raster data, are documented with the :ref:`python_raster_api`.

- `Layer`_
- `Feature`_
- `Geometry`_
- `FeatureDefn`_
- `FieldDefn`_
- `GeomFieldDefn`_
- `FieldDomain`_
- `Relationship`_
- `StyleTable`_

Layer
-----

.. autoclass:: osgeo.ogr.Layer
    :members:
    :undoc-members:
    :exclude-members: thisown

Feature
-------

.. autoclass:: osgeo.ogr.Feature
    :members:
    :undoc-members:
    :exclude-members: thisown

Geometry
--------

.. autoclass:: osgeo.ogr.Geometry
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autofunction:: osgeo.ogr.CreateGeometryFromEsriJson

.. autofunction:: osgeo.ogr.CreateGeometryFromGML

.. autofunction:: osgeo.ogr.CreateGeometryFromJson

.. autofunction:: osgeo.ogr.CreateGeometryFromWkb

.. autofunction:: osgeo.ogr.CreateGeometryFromWkt

.. autofunction:: osgeo.ogr.ForceTo

.. autofunction:: osgeo.ogr.ForceToLineString

.. autofunction:: osgeo.ogr.ForceToMultiLineString

.. autofunction:: osgeo.ogr.ForceToMultiPoint

.. autofunction:: osgeo.ogr.ForceToMultiPolygon

.. autofunction:: osgeo.ogr.ForceToPolygon

.. autofunction:: osgeo.ogr.GeometryTypeToName

.. autofunction:: osgeo.ogr.GT_Flatten

.. autofunction:: osgeo.ogr.GT_GetCollection

.. autofunction:: osgeo.ogr.GT_GetCurve

.. autofunction:: osgeo.ogr.GT_GetLinear

.. autofunction:: osgeo.ogr.GT_HasM

.. autofunction:: osgeo.ogr.GT_HasZ

.. autofunction:: osgeo.ogr.GT_IsCurve

.. autofunction:: osgeo.ogr.GT_IsNonLinear

.. autofunction:: osgeo.ogr.GT_IsSubClassOf

.. autofunction:: osgeo.ogr.GT_IsSurface

.. autofunction:: osgeo.ogr.GT_SetM

.. autofunction:: osgeo.ogr.GT_SetModifier

.. autofunction:: osgeo.ogr.GT_SetZ

FeatureDefn
-----------

.. autoclass:: osgeo.ogr.FeatureDefn
    :members:
    :undoc-members:
    :exclude-members: thisown

FieldDefn
---------

.. autoclass:: osgeo.ogr.FieldDefn
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autofunction:: osgeo.ogr.GetFieldSubTypeName

.. autofunction:: osgeo.ogr.GetFieldTypeName

GeomFieldDefn
-------------

.. autoclass:: osgeo.ogr.GeomFieldDefn
    :members:
    :undoc-members:
    :exclude-members: thisown

FieldDomain
-----------

.. autoclass:: osgeo.ogr.FieldDomain
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autofunction:: osgeo.ogr.CreateCodedFieldDomain

.. autofunction:: osgeo.ogr.CreateGlobFieldDomain

.. autofunction:: osgeo.ogr.CreateRangeFieldDomain

Relationship
------------

.. autoclass:: osgeo.gdal.Relationship
    :members:
    :undoc-members:
    :exclude-members: thisown

StyleTable
----------

.. autoclass:: osgeo.ogr.StyleTable
    :members:
    :undoc-members:
    :exclude-members: thisown

