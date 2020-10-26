#!/usr/bin/env python3

import sys
# import osgeo.utils.pct2rgb as a convenience to use as a script
from osgeo.utils.pct2rgb import *  # noqa
from osgeo.utils.pct2rgb import main
from osgeo.gdal import deprecation_warn


deprecation_warn('pct2rgb', 'utils')
sys.exit(main(sys.argv))
