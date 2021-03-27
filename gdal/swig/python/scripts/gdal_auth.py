#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal_auth as a convenience to use as a script
from osgeo_utils.gdal_auth import *  # noqa
from osgeo_utils.gdal_auth import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_auth')
sys.exit(main(sys.argv))
