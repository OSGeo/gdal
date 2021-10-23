from numpy import *
from osgeo.gdal_array import *

from warnings import warn

warn('instead of `import gdalnumeric`, please consider `import numpy` and/or `from osgeo import gdal_array`',
     DeprecationWarning)
