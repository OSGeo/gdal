#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal_merge as a convenience to use as a script
from osgeo_utils.gdal_merge import *  # noqa
from osgeo_utils.gdal_merge import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_merge')
sys.exit(main(sys.argv))
