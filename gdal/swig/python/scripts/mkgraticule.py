#!/usr/bin/env python3

import sys
# import osgeo.utils.mkgraticule as a convenience to use as a script
from osgeo.utils.mkgraticule import *  # noqa
from osgeo.utils.mkgraticule import main
from osgeo.gdal import deprecation_warn


deprecation_warn('mkgraticule', 'utils')
sys.exit(main(sys.argv))
