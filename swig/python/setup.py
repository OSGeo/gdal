#!/usr/bin/env python

import sys

from distutils.core import setup, Extension

include_dirs = ['../../port',
                '../../gcore',
                '../../alg']


# Put your build libraries and lib link directories here
# These *must* match what is in nmake.opt.  We could get
# fancier here and parse the nmake.opt for lib files in the future.
if sys.platform == 'win32':
    libraries = ['gdal',
                 'xerces-c_2',
                 'libjasper',
                 'lti_dsdk_dll',
                 'netcdf',
                 'proj']
    library_dirs = ['../../',
                    r'D:\cvs\gdal\xerces\lib',
                    r'D:\cvs\gdal\jasper\jasper-1.701.0.uuid\src\msvc\Win32_Release',
                    r'd:\cvs\gdal\mrsid\lib\Release_md',
                    r'd:\cvs\gdal\netcdf\lib',
                    r'd:\cvs\proj\src']
else:
    libraries = ['gdal']
    library_dirs = ['../../']


gdal_module = Extension('_gdal',
                    sources=['gdal_wrap.cpp'],
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs)


setup( name = 'Gdal Wrapper',
       version = 'ng using swig 1.3',
       description = 'Swig 1.3 wrapper over gdal',
       py_modules = ['gdal'],
       url="http://gdal.maptools.org",
       ext_modules = [gdal_module] )
