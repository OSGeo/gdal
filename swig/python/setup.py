#!/usr/bin/env python

from distutils.core import setup, Extension

module1 = Extension('_gdal',
                    sources=['gdal_wrap.cpp'],
                    include_dirs = [ '../' ],
                    libraries = ['gdal'],
                    library_dirs = ['/usr/local/lib'] )

setup( name = 'Gdal Wrapper',
       version = 'ng using swig 1.3',
       description = 'Swig 1.3 wrapper over gdal',
       ext_modules = [module1] )
