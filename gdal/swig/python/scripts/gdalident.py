#!/usr/bin/env python3

import sys
# import osgeo.utils.gdalident as a convenience to use as a script
from osgeo.utils.gdalident import *  # noqa
from osgeo.utils.gdalident import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalident', 'utils')
sys.exit(main(sys.argv))
