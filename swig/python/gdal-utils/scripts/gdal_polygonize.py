#!/usr/bin/env python3

import sys

from osgeo.gdal import deprecation_warn

# import osgeo_utils.gdal_polygonize as a convenience to use as a script
from osgeo_utils.gdal_polygonize import *  # noqa
from osgeo_utils.gdal_polygonize import main

deprecation_warn("gdal_polygonize")
sys.exit(main(sys.argv))
