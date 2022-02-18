#!/usr/bin/env python3

import sys
# import osgeo_utils.gdal2tiles as a convenience to use as a script
from osgeo_utils.gdal2tiles import *  # noqa
from osgeo_utils.gdal2tiles import main
from osgeo.gdal import deprecation_warn

# Running main() must be protected that way due to use of multiprocessing on Windows:
# https://docs.python.org/3/library/multiprocessing.html#the-spawn-and-forkserver-start-methods
if __name__ == '__main__':
    if '--mpi' in sys.argv:
        from runpy import run_module
        sys.argv[:] = ['mpi4py.futures', '-m', 'osgeo_utils.gdal2tiles'] + sys.argv[1:]
        run_module(sys.argv[0], run_name='__main__', alter_sys=True)
        sys.exit(0)

    # import osgeo_utils.gdal2tiles as a convenience to use as a script
    # pyrun.run_module above issues warning if osgeo_utils.gdal2tiles is already loaded 
    from osgeo_utils.gdal2tiles import *  # noqa
    from osgeo_utils.gdal2tiles import main
    from osgeo.gdal import deprecation_warn

    # Trick inspired from https://stackoverflow.com/questions/45720153/python-multiprocessing-error-attributeerror-module-main-has-no-attribute
    # and https://bugs.python.org/issue42949
    __spec__ = None
    deprecation_warn('gdal2tiles')
    sys.exit(main(sys.argv))
