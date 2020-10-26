#!/usr/bin/env python3

import sys
# import osgeo.utils.rgb2pct as a convenience to use as a script
from osgeo.utils.rgb2pct import *  # noqa
from osgeo.utils.rgb2pct import main
from osgeo.gdal import deprecation_warn


deprecation_warn('rgb2pct', 'utils')
sys.exit(main(sys.argv))
