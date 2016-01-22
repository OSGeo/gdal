# import osgeo.gdal_array as a convenience
from osgeo.gdal import deprecation_warn
deprecation_warn('gdalnumeric')

from osgeo.gdal_array import *
