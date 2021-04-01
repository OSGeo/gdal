#!/usr/bin/env python3

import sys
# import osgeo_utils.esri2wkt as a convenience to use as a script
from osgeo_utils.esri2wkt import *  # noqa
from osgeo_utils.esri2wkt import main
from osgeo.gdal import deprecation_warn


deprecation_warn('esri2wkt')
sys.exit(main(sys.argv))
