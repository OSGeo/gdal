#!/usr/bin/env python3

import sys
# import osgeo_utils.gcps2wld as a convenience to use as a script
from osgeo_utils.gcps2wld import *  # noqa
from osgeo_utils.gcps2wld import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gcps2wld')
sys.exit(main(sys.argv))
