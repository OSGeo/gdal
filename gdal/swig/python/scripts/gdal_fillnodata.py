#!/usr/bin/env python3

import sys
# import osgeo.utils.gdal_fillnodata as a convenience to use as a script
from osgeo.utils.gdal_fillnodata import *  # noqa
from osgeo.utils.gdal_fillnodata import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_fillnodata', 'utils')
sys.exit(main(sys.argv))
