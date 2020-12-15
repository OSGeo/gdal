#!/usr/bin/env python3

import sys
# import osgeo.utils.gdal_merge as a convenience to use as a script
from osgeo.utils.gdal_merge import *  # noqa
from osgeo.utils.gdal_merge import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_merge', 'utils')
sys.exit(main(sys.argv))
