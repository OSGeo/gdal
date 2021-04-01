#!/usr/bin/env python3

import sys
# import osgeo_utils.gdalchksum as a convenience to use as a script
from osgeo_utils.gdalchksum import *  # noqa
from osgeo_utils.gdalchksum import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalchksum')
sys.exit(main(sys.argv))
