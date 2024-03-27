.. _python_raster_api:

Python Raster API
=================

This page contains classes, methods, functions that relate to the GDAL :ref:`raster_data_model`:

- `Driver`_
- `Dataset`_
- `Band`_
- `Other`_

.. py:currentmodule:: osgeo.gdal

Driver
------

.. autoclass:: osgeo.gdal.Driver
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autofunction:: osgeo.gdal.AllRegister

.. autofunction:: osgeo.gdal.GetDriver

.. autofunction:: osgeo.gdal.GetDriverByName

.. autofunction:: osgeo.gdal.GetDriverCount

.. autofunction:: osgeo.gdal.IdentifyDriver

.. autofunction:: osgeo.gdal.IdentifyDriverEx

Dataset
-------

.. autoclass:: osgeo.gdal.Dataset
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autofunction:: osgeo.gdal.Open

.. autofunction:: osgeo.gdal.OpenEx

.. autofunction:: osgeo.gdal.OpenShared

Band
----

.. autoclass:: osgeo.gdal.Band
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autofunction:: osgeo.gdal.RegenerateOverview

.. autofunction:: osgeo.gdal.RegenerateOverviews

Other
-----

.. autoclass:: osgeo.gdal.RasterAttributeTable
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autoclass:: osgeo.gdal.ColorTable
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autoclass:: osgeo.gdal.ColorEntry
    :members:
    :undoc-members:
    :exclude-members: thisown

.. autoclass:: osgeo.gdal.GCP
    :members:
    :undoc-members:
    :exclude-members: thisown
