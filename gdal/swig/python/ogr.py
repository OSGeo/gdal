# import osgeo.ogr as a convenience
from osgeo.gdal import deprecation_warn
deprecation_warn('ogr')

from osgeo.ogr import *
