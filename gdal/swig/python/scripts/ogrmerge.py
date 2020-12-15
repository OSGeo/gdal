#!/usr/bin/env python3

import sys
# import osgeo.utils.ogrmerge as a convenience to use as a script
from osgeo.utils.ogrmerge import *  # noqa
from osgeo.utils.ogrmerge import main
from osgeo.gdal import deprecation_warn


deprecation_warn('ogrmerge', 'utils')
sys.exit(main(sys.argv))
