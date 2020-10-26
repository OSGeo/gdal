#!/usr/bin/env python3

import sys
# import osgeo.utils.gdal_retile as a convenience to use as a script
from osgeo.utils.gdal_retile import *  # noqa
from osgeo.utils.gdal_retile import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdal_retile', 'utils')
sys.exit(main(sys.argv))
