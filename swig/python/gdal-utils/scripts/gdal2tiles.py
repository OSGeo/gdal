#!/usr/bin/env python3

import sys

# Running main() must be protected that way due to use of multiprocessing on Windows:
# https://docs.python.org/3/library/multiprocessing.html#the-spawn-and-forkserver-start-methods
if __name__ == "__main__":
    from osgeo.gdal import deprecation_warn

    # import osgeo_utils.gdal2tiles as a convenience to use as a script
    from osgeo_utils.gdal2tiles import *  # noqa
    from osgeo_utils.gdal2tiles import main

    deprecation_warn("gdal2tiles")
    sys.exit(main(sys.argv, called_from_main=True))
