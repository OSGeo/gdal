# import osgeo.gdal_array as a convenience
from warnings import warn
warn('gdalnumeric.py was placed in a namespace, it is now available as osgeo.gdal_array', DeprecationWarning)

from osgeo.gdal_array import *
