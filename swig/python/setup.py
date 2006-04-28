#!/usr/bin/env python

import sys
import os
import string

from os import path
from glob import glob
from distutils.core import setup, Extension
from distutils.sysconfig import parse_makefile,expand_makefile_vars

from distutils.core import setup, Extension

# Function needed to make unique lists.
def unique(list):
    """Stolen from MapScript setup script"""
    dict = {}
    for item in list:
        dict[item] = ''
    return dict.keys()


include_dirs = ['../../port',
                '../../gcore',
                '../../alg',
                '../../ogr']


# Put your build libraries and lib link directories here
# These *must* match what is in nmake.opt.  We could get
# fancier here and parse the nmake.opt for lib files in the future.
if sys.platform == 'win32':
    # Created by the GDAL build process.
    # This was swiped from MapServer
    setupvars = "../setup.ini"

    # Open and read lines from setup.ini.
    try:
      fp = open(setupvars, "r")
    except IOError, e:
      raise IOError, '%s. %s' % (e, "Has GDAL been made?")
    gdal_basedir = fp.readline()
    gdal_version = fp.readline()
    gdal_libs = fp.readline()
    gdal_includes = fp.readline()
    lib_opts = gdal_libs.split()
    library_dirs = [x[2:] for x in lib_opts if x[:2] == "-L"]
    library_dirs = unique(library_dirs)
    library_dirs = library_dirs + gdal_basedir.split()
    libraries = []
    extras = []

    for x in lib_opts:
        if x[:2] == '-l':
            libraries.append( x[2:] )
        if x[-4:] == '.lib' or x[-4:] == '.LIB':
          dir, lib = os.path.split(x)
          libraries.append( lib[:-4] )
          if len(dir) > 0:
              library_dirs.append( dir )
        if x[-2:] == '.a':
            extras.append(x)
            
    # don't forget to add gdal to the list :)
    libraries.append('gdal_i')
    extra_link_args = []#['/NODEFAULTLIB:MSVCRT']

elif sys.platform == 'cygwin':
    TOP_DIR = "../.."
     
    DICT = parse_makefile(os.path.join(TOP_DIR,"GDALMake.opt"))
     
    library_dirs = [TOP_DIR+"/","./"]
    prefix = string.split(DICT[expand_makefile_vars("prefix",DICT)])[0]
    prefix_lib = prefix + "/lib"
    library_dirs.append(prefix_lib)
     
    extra_link_args = []
     
    print "\nLIBRARY_DIRS:\n\t",library_dirs
     
    libraries = ["gdal"]
    gdal_libs = ["LIBS","PG_LIB","MYSQL_LIB"]
    for gdal_lib in gdal_libs:
        for lib in string.split(DICT[expand_makefile_vars(gdal_lib,DICT)]):
            if lib[0:2] == "-l":
                libraries.append(lib[2:])
    libraries.append("stdc++")
     
    print "\nLIBRARIES:\n\t",libraries
     
    include_dirs=[path.join(TOP_DIR,"gcore"), path.join(TOP_DIR,"port"),
        path.join(TOP_DIR,"ogr"), path.join(TOP_DIR,"pymod"), ] # only necessary
     
    include_files = [
      glob(path.join(TOP_DIR,"gcore", "*.h")),
      glob(path.join(TOP_DIR,"port", "*.h")),
      glob(path.join(TOP_DIR,"alg", "*.h")),
      glob(path.join(TOP_DIR,"ogr", "*.h")), 
      glob(path.join(TOP_DIR,"ogr", "ogrsf_frmts", "*.h"))
            ]
     
    IF=[]
    for i in include_files:
      IF.extend(i)
    include_files=IF
    del IF
     
    print "\nINCLUDE_FILES:",include_files
else:
    libraries = ['gdal']
    library_dirs = ['../../', '../../.libs']
    extra_link_args = []


gdal_module = Extension('_gdal',
                    sources=['gdal_wrap.cpp'],
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    extra_link_args = extra_link_args)

gdalconst_module = Extension('_gdalconst',
                    sources=['gdalconst_wrap.c'],
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    extra_link_args = extra_link_args)

osr_module = Extension('_osr',
                    sources=['osr_wrap.cpp'],
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    extra_link_args = extra_link_args)

ogr_module = Extension('_ogr',
                    sources=['ogr_wrap.cpp'],
                    include_dirs = include_dirs,
                    libraries = libraries,
                    library_dirs = library_dirs,
                    extra_link_args = extra_link_args)


setup( name = 'Gdal Wrapper',
       version = 'ng using swig 1.3',
       description = 'Swig 1.3 wrapper over gdal',
       py_modules = ['gdal', 'osr', 'ogr','gdalconst'],
       url="http://www.gdal.org",
       ext_modules = [gdal_module,
                      gdalconst_module,
                      osr_module,
                      ogr_module],debug=1 )
