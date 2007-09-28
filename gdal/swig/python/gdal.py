# import osgeo.gdal as a convenience
from warnings import warn
warn('gdal.py was placed in a namespace, it is now available as osgeo.gdal', DeprecationWarning)

from osgeo.gdal import *
