#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal_edit as a convenience to use as a script
from osgeo_utils.gdal_edit import *  # noqa
from osgeo_utils.gdal_edit import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_edit')
sys.exit(main(sys.argv))
