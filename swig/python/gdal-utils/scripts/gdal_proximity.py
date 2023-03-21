#!/usr/bin/env python3

import sys

from osgeo.gdal import UseExceptionsAllModules, deprecation_warn

# import osgeo_utils.gdal_proximity as a convenience to use as a script
from osgeo_utils.gdal_proximity import *  # noqa
from osgeo_utils.gdal_proximity import main

UseExceptionsAllModules()

deprecation_warn("gdal_proximity")
sys.exit(main(sys.argv))
