#!/usr/bin/env python3

import sys
# import osgeo_utils.gdalimport as a convenience to use as a script
from osgeo_utils.gdalimport import *  # noqa
from osgeo_utils.gdalimport import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalimport')
sys.exit(main(sys.argv))
