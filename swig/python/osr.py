# import osgeo.osr as a convenience
from osgeo.osr import *
from osgeo import gdal

gdal.Debug("Deprecation Warning", 'osr.py was placed in a namespace, it is now available as osgeo.osr')
