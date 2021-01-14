#!/usr/bin/env python3

import sys
# import osgeo.utils.gdalattachpct as a convenience to use as a script
from osgeo.utils.gdalattachpct import *  # noqa
from osgeo.utils.gdalattachpct import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalattachpct', 'utils')
sys.exit(main(sys.argv))
