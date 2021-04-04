#!/usr/bin/env python3

import sys
# import osgeo_utils.mkgraticule as a convenience to use as a script
from osgeo_utils.mkgraticule import *  # noqa
from osgeo_utils.mkgraticule import main
from osgeo.gdal import deprecation_warn


deprecation_warn('mkgraticule')
sys.exit(main(sys.argv))
