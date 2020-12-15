#!/usr/bin/env python3

import sys
# import osgeo.utils.gdal_polygonize as a convenience to use as a script
from osgeo.utils.gdal_polygonize import *  # noqa
from osgeo.utils.gdal_polygonize import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_polygonize', 'utils')
sys.exit(main(sys.argv))
