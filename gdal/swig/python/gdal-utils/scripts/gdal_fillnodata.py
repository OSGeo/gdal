#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal_fillnodata as a convenience to use as a script
from osgeo_utils.gdal_fillnodata import *  # noqa
from osgeo_utils.gdal_fillnodata import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_fillnodata')
sys.exit(main(sys.argv))
