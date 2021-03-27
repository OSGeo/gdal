#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal_polygonize as a convenience to use as a script
from osgeo_utils.gdal_polygonize import *  # noqa
from osgeo_utils.gdal_polygonize import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_polygonize')
sys.exit(main(sys.argv))
