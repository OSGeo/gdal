# import osgeo.gdalconst as a convenience
from osgeo.gdalconst import *
from osgeo import gdal

gdal.Debug("Deprecation Warning", 'gdalconst.py was placed in a namespace, it is now available as osgeo.gdalconst')
