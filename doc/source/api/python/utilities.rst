..
   The documentation displayed on this page is automatically generated from
   Python docstrings. See https://gdal.org/development/dev_documentation.html
   for information on updating this content.

.. _python_utilities:

Utilities / Algorithms API
==========================

Raster Utilities
----------------

.. autofunction:: osgeo.gdal.AutoCreateWarpedVRT

.. autofunction:: osgeo.gdal.BuildVRT

.. autofunction:: osgeo.gdal.BuildVRTOptions

.. autofunction:: osgeo.gdal.ComputeProximity

.. autofunction:: osgeo.gdal.Contour

.. autofunction:: osgeo.gdal.ContourOptions

.. autofunction:: osgeo.gdal.ContourGenerate

.. autofunction:: osgeo.gdal.ContourGenerateEx

.. autofunction:: osgeo.gdal.CreatePansharpenedVRT

.. autofunction:: osgeo.gdal.DEMProcessing

.. autofunction:: osgeo.gdal.DEMProcessingOptions

.. autofunction:: osgeo.gdal.Footprint

.. autofunction:: osgeo.gdal.FootprintOptions

.. autofunction:: osgeo.gdal.FillNodata

.. autofunction:: osgeo.gdal.Grid

.. autofunction:: osgeo.gdal.GridOptions

.. autofunction:: osgeo.gdal.Info

.. autofunction:: osgeo.gdal.InfoOptions

.. autofunction:: osgeo.gdal.Nearblack

.. autofunction:: osgeo.gdal.NearblackOptions

.. autofunction:: osgeo.gdal.Polygonize

.. autofunction:: osgeo.gdal.Rasterize

.. autofunction:: osgeo.gdal.RasterizeOptions

.. autofunction:: osgeo.gdal.RasterizeLayer

.. autofunction:: osgeo.gdal.SieveFilter

.. autofunction:: osgeo.gdal.SuggestedWarpOutput

.. autofunction:: osgeo.gdal.TileIndex

.. autofunction:: osgeo.gdal.TileIndexOptions

.. autofunction:: osgeo.gdal.Translate

.. autofunction:: osgeo.gdal.TranslateOptions

.. autofunction:: osgeo.gdal.ViewshedGenerate

.. autofunction:: osgeo.gdal.Warp

.. autofunction:: osgeo.gdal.WarpOptions

Multidimensional Raster Utilities
---------------------------------

.. autofunction:: osgeo.gdal.MultiDimInfo

.. autofunction:: osgeo.gdal.MultiDimInfoOptions

.. autofunction:: osgeo.gdal.MultiDimTranslate

.. autofunction:: osgeo.gdal.MultiDimTranslateOptions

Vector Utilities
----------------

.. autofunction:: osgeo.gdal.VectorInfo

.. autofunction:: osgeo.gdal.VectorInfoOptions

.. autofunction:: osgeo.gdal.VectorTranslate

.. autofunction:: osgeo.gdal.VectorTranslateOptions

Algorithms
----------

.. autoclass:: osgeo.gdal.Algorithm
   :members:
   :exclude-members: thisown

.. autoclass:: osgeo.gdal.AlgorithmArg
   :members:
   :exclude-members: thisown

.. autofunction:: osgeo.gdal.AlgorithmArgTypeIsList

.. autofunction:: osgeo.gdal.AlgorithmArgTypeName

.. autoclass:: osgeo.gdal.AlgorithmRegistry
   :members:
   :exclude-members: thisown
   
.. autofunction:: osgeo.gdal.GetGlobalAlgorithmRegistry

.. autofunction:: osgeo.gdal.Run
