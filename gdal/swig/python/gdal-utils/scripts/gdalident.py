#!/usr/bin/env python3

import sys
# import osgeo_utils.gdalident as a convenience to use as a script
from osgeo_utils.gdalident import *  # noqa
from osgeo_utils.gdalident import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalident')
sys.exit(main(sys.argv))
