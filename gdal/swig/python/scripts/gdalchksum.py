#!/usr/bin/env python3

import sys
# import osgeo.utils.gdalchksum as a convenience to use as a script
from osgeo.utils.gdalchksum import *  # noqa
from osgeo.utils.gdalchksum import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalchksum')
sys.exit(main(sys.argv))
