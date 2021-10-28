#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal_sieve as a convenience to use as a script
from osgeo_utils.gdal_sieve import *  # noqa
from osgeo_utils.gdal_sieve import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_sieve')
sys.exit(main(sys.argv))
