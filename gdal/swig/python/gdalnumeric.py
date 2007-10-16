# import osgeo.gdal_array as a convenience
from osgeo.gdal_array import *
from osgeo import gdal

gdal.Debug("Deprecation Warning", 'gdalnumeric.py was placed in a namespace, it is now available as osgeo.gdal_array')
