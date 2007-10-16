# import osgeo.ogr as a convenience
from osgeo.ogr import *
from osgeo import gdal

gdal.Debug("Deprecation Warning", 'ogr.py was placed in a namespace, it is now available as osgeo.ogr')
