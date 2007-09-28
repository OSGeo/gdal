# import osgeo.gdalconst as a convenience
from warnings import warn
warn('gdalconst.py was placed in a namespace, it is now available as osgeo.gdalconst', DeprecationWarning)

from osgeo.gdalconst import *
