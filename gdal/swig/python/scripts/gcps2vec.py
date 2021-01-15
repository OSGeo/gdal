#!/usr/bin/env python3

import sys
# import osgeo_utils.gcps2vec as a convenience to use as a script
from osgeo_utils.gcps2vec import *  # noqa
from osgeo_utils.gcps2vec import main
from osgeo.gdal import deprecation_warn


deprecation_warn('gcps2vec')
sys.exit(main(sys.argv))
