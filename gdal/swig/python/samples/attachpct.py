#!/usr/bin/env python3

import sys
# import osgeo.utils.attachpct as a convenience to use as a script
from osgeo.utils.attachpct import *  # noqa
from osgeo.utils.attachpct import main
from osgeo.gdal import deprecation_warn


deprecation_warn('attachpct', 'utils')
sys.exit(main(sys.argv))
