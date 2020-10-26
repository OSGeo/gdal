#!/usr/bin/env python3

import sys
# import osgeo.utils.gdal2xyz as a convenience to use as a script
from osgeo.utils.gdal2xyz import *  # noqa
from osgeo.utils.gdal2xyz import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal2xyz', 'utils')
sys.exit(main(sys.argv))
