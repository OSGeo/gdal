#!/usr/bin/env python3

import sys
# import osgeo.utils.epsg_tr as a convenience to use as a script
from osgeo.utils.epsg_tr import *  # noqa
from osgeo.utils.epsg_tr import main
from osgeo.gdal import deprecation_warn


deprecation_warn('epsg_tr', 'utils')
sys.exit(main(sys.argv))
