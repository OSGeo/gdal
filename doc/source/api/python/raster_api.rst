..
   The documentation displayed on this page is automatically generated from
   Python docstrings. See https://gdal.org/development/dev_documentation.html
   for information on updating this content.

.. _python_raster_api:

Raster API
==========

This page contains classes, methods, functions that relate to the GDAL :ref:`raster_data_model`:

- `Driver`_
- `Dataset`_
- `Band`_
- `Band Algebra`_
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

Band Algebra
------------

.. autoclass:: osgeo.gdal.ComputedBand

.. autofunction:: osgeo.gdal.abs

.. autofunction:: osgeo.gdal.log

.. autofunction:: osgeo.gdal.log10

.. autofunction:: osgeo.gdal.logical_and

.. autofunction:: osgeo.gdal.logical_or

.. autofunction:: osgeo.gdal.logical_not

.. autofunction:: osgeo.gdal.maximum

.. autofunction:: osgeo.gdal.mean

.. autofunction:: osgeo.gdal.minimum

.. autofunction:: osgeo.gdal.pow

.. autofunction:: osgeo.gdal.sqrt

.. autofunction:: osgeo.gdal.where

Other
-----

.. autofunction:: osgeo.gdal.ApplyGeoTransform

.. autofunction:: osgeo.gdal.InvGeoTransform

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


.. below is an allow-list for spelling checker.

.. spelling:word-list::
   RasterCount
