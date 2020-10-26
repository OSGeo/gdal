#!/usr/bin/env python3

import sys
# import osgeo.utils.gdalmove as a convenience to use as a script
from osgeo.utils.gdalmove import *  # noqa
from osgeo.utils.gdalmove import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gdalmove', 'utils')
sys.exit(main(sys.argv))
