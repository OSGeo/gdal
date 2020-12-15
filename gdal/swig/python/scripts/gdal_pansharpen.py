#!/usr/bin/env python3

import sys
# import osgeo.utils.gdal_pansharpen as a convenience to use as a script
from osgeo.utils.gdal_pansharpen import *  # noqa
from osgeo.utils.gdal_pansharpen import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_pansharpen', 'utils')
sys.exit(main(sys.argv))
