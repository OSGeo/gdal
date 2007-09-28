# import osgeo.ogr as a convenience
from warnings import warn
warn('ogr.py was placed in a namespace, it is now available as osgeo.ogr', DeprecationWarning)

from osgeo.ogr import *
