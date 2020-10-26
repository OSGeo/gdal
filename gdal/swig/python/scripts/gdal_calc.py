#!/usr/bin/env python3

import sys
# import osgeo.utils.gdal_calc as a convenience to use as a script
from osgeo.utils.gdal_calc import *  # noqa
from osgeo.utils.gdal_calc import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_calc', 'utils')
sys.exit(main(sys.argv))
