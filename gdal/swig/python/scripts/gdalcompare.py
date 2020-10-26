#!/usr/bin/env python3

import sys
# import osgeo.utils.gdalcompare as a convenience to use as a script
from osgeo.utils.gdalcompare import *  # noqa
from osgeo.utils.gdalcompare import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalcompare', 'utils')
sys.exit(main(sys.argv))
