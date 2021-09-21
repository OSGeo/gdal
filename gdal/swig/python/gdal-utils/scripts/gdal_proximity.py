#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal_proximity as a convenience to use as a script
from osgeo_utils.gdal_proximity import *  # noqa
from osgeo_utils.gdal_proximity import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_proximity')
sys.exit(main(sys.argv))
