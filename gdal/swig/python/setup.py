#!/usr/bin/env python

import sys
import os
import string

from glob import glob

# ---------------------------------------------------------------------------
# Switches
# ---------------------------------------------------------------------------

HAVE_NUMPY=False
HAVE_SETUPTOOLS = False
BUILD_FOR_CHEESESHOP = False
DEFAULT_GDAL_CONFIG = 'gdal-config'

# ---------------------------------------------------------------------------
# Helper Functions
# ---------------------------------------------------------------------------

# Function to find numpy's include directory
def get_numpy_include():
    if HAVE_NUMPY:
        return numpy.get_include()
    else:
        return '.'

# Function needed to make unique lists.
def unique(list):
    """Stolen from MapScript setup script"""
    dict = {}
    for item in list:
        dict[item] = ''
    return dict.keys()


# ---------------------------------------------------------------------------
# Imports
# ---------------------------------------------------------------------------

try:
    import numpy
    HAVE_NUMPY = True
    # check version
    numpy_major = numpy.__version__.split('.')[0]
    if int(numpy_major) < 1:
        print "numpy version must be > 1.0.0"
        HAVE_NUMPY = False
    else:
        print 'numpy include', get_numpy_include()
        if get_numpy_include() =='.':
            print "numpy headers were not found!  Array support will not be enabled"
            HAVE_NUMPY=False
except ImportError:
    pass



try:
    from setuptools import setup
    from setuptools import Extension
    HAVE_SETUPTOOLS = True
except ImportError:
    from distutils.core import setup, Extension

from distutils.sysconfig import parse_makefile
from distutils.sysconfig import expand_makefile_vars

from distutils.command.build_ext import build_ext
from distutils.ccompiler import get_default_compiler
from distutils.sysconfig import get_python_inc
from distutils.errors import DistutilsFileError

import popen2


def get_gdal_config(kind, gdal_config='gdal-config'):
    import popen2
    p = popen2.popen3(gdal_config + " --%s" % kind)
    r = p[0].readline().strip()
    if not r:
        raise Warning(p[2].readline())
    return r  
    
class gdal_build_ext(build_ext):

    user_options = build_ext.user_options[:]
    user_options.extend([
        ('gdal-config', None,
        "The name of the gdal-config binary and/or a full path to it"),
    ])
    
    def initialize_options(self):
        build_ext.initialize_options(self)
        self.gdal_config = DEFAULT_GDAL_CONFIG
        self.numpy_include_dir = get_numpy_include()
        self.gdaldir = get_gdal_config('prefix', gdal_config = self.gdal_config)

    def get_compiler(self):
        return self.compiler or get_default_compiler()
    
    def get_gdal_config(self, kind):
        return get_gdal_config(kind, gdal_config =self.gdal_config)
    
    def finalize_win32(self):
        if self.get_compiler() == 'msvc':
            # Created by the GDAL build process.
            # This was swiped from MapServer
            setupvars = "../setup.ini"

            # Open and read lines from setup.ini.
            try:
              fp = open(setupvars, "r")
            except IOError, e:
              raise IOError, '%s. %s' % (e, "Has GDAL been made?")
            gdal_basedir = fp.readline()
            old_version = fp.readline()
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
            self.libraries.remove('gdal')
            self.libraries.append('gdal_i')
        build_ext.finalize_win32()
    
    def finalize_options(self):
        build_ext.finalize_options(self)
        self.include_dirs.append('.')
        self.include_dirs.append(self.numpy_include_dir)

        try:
            self.library_dirs.append(os.path.join(self.gdaldir,'lib'))
            self.include_dirs.append(os.path.join(self.gdaldir,'include'))
            version = self.get_gdal_config('version')
            gdalmajor, gdalminor, gdalpatch = version.split('.')
        except Warning, w:
            if self.gdal_config == self.DEFAULT_GDAL_CONFIG:
                sys.stderr.write("Warning: %s" % str(w))
            else:
                sys.stderr.write("Error: %s" % str(w))
                sys.exit(1)
# ---------------------------------------------------------------------------
# Platform specifics
# ---------------------------------------------------------------------------



if sys.platform == 'cygwin':
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
     
    include_dirs=[os.path.join(TOP_DIR,"gcore"), os.path.join(TOP_DIR,"port"),
        os.path.join(TOP_DIR,"ogr"), os.path.join(TOP_DIR,"pymod"), ] # only necessary
     
    include_files = [
      glob(os.path.join(TOP_DIR,"gcore", "*.h")),
      glob(os.path.join(TOP_DIR,"port", "*.h")),
      glob(os.path.join(TOP_DIR,"alg", "*.h")),
      glob(os.path.join(TOP_DIR,"ogr", "*.h")), 
      glob(os.path.join(TOP_DIR,"ogr", "ogrsf_frmts", "*.h"))
            ]
     
    IF=[]
    for i in include_files:
      IF.extend(i)
    include_files=IF
    del IF
     
    print "\nINCLUDE_FILES:",include_files
# else:
#     libraries = ['gdal']
#     library_dirs = ['../../.libs','../..','/usr/lib','/usr/local/lib']

extra_link_args = []


# ---------------------------------------------------------------------------
# Platform specifics
# ---------------------------------------------------------------------------


gdal_version = '1.5.0.a.dev'

# include_dirs = ['../../port',
#                 '../../gcore',
#                 '../../alg',
#                 '../../ogr',
#                 get_numpy_include(),
#                 '/usr/include',
#                 '/usr/local/include']
                
extra_compile_args = []

# might need to tweak for Python 2.4 on OSX to be these
#extra_compile_args = ['-g', '-arch', 'i386', '-isysroot','/']

gdal_module = Extension('osgeo._gdal',
                        sources=['extensions/gdal_wrap.cpp'],
                        # include_dirs = include_dirs,
                        # libraries = libraries,
                        # library_dirs = library_dirs,
                        extra_compile_args = extra_compile_args,
                        extra_link_args = extra_link_args)

gdalconst_module = Extension('osgeo._gdalconst',
                    sources=['extensions/gdalconst_wrap.c'],
                    # include_dirs = include_dirs,
                    # libraries = libraries,
                    # library_dirs = library_dirs,
                    extra_compile_args = extra_compile_args,
                    extra_link_args = extra_link_args)

osr_module = Extension('osgeo._osr',
                    sources=['extensions/osr_wrap.cpp'],
                    # include_dirs = include_dirs,
                    # libraries = libraries,
                    # library_dirs = library_dirs,
                    extra_compile_args = extra_compile_args,
                    extra_link_args = extra_link_args)

ogr_module = Extension('osgeo._ogr',
                    sources=['extensions/ogr_wrap.cpp'],
                    # include_dirs = include_dirs,
                    # libraries = libraries,
                    # library_dirs = library_dirs,
                    extra_compile_args = extra_compile_args,
                    extra_link_args = extra_link_args)


array_module = Extension('osgeo._gdal_array',
                    sources=['extensions/_gdal_array.cpp'],
                    # include_dirs = include_dirs,
                    # libraries = libraries,
                    # library_dirs = library_dirs,
                    extra_compile_args = extra_compile_args,
                    extra_link_args = extra_link_args)

ext_modules = [gdal_module,
              gdalconst_module,
              osr_module,
              ogr_module]

py_modules = ['gdal',
              'ogr',
              'osr',
              'gdalconst']
      
if HAVE_NUMPY:
    ext_modules.append(array_module)
    py_modules.append('gdalnumeric')

packages = ["osgeo",]

readme = file('README.txt','rb').read()

name = 'GDAL'
version = gdal_version
author = "Frank Warmerdam"
author_email = "warmerdam@pobox.com"
maintainer = "Howard Butler"
maintainer_email = "hobu.inc@gmail.com"
description = "GDAL: Geospatial Data Abstraction Library"
license = "MIT"
url="http://www.gdal.org"

classifiers = [
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python',
        'Programming Language :: C',
        'Programming Language :: C++',
        'Topic :: Scientific/Engineering :: GIS',
        'Topic :: Scientific/Engineering :: Information Analysis',
        
]



if BUILD_FOR_CHEESESHOP:
    data_files = [("osgeo/data/gdal", glob(os.path.join("../../data", "*")))]
else:
    data_files = None
    
exclude_package_data = {'':['GNUmakefile']}

if HAVE_SETUPTOOLS:  
    setup( name = name,
           version = gdal_version,
           author = author,
           author_email = author_email,
           maintainer = maintainer,
           maintainer_email = maintainer_email,
           long_description = readme,
           description = description,
           license = license,
           classifiers = classifiers,
           py_modules = py_modules,
           packages = packages,
           url=url,
           data_files = data_files,
           zip_safe = False,
           exclude_package_data = exclude_package_data,
           install_requires =['numpy>=1.0.0'],
           cmdclass={'build_ext':gdal_build_ext},
           ext_modules = ext_modules )
else:
    setup( name = name,
           version = gdal_version,
           author = author,
           author_email = author_email,
           maintainer = maintainer,
           maintainer_email = maintainer_email,
           long_description = readme,
           description = description,
           license = license,
           classifiers = classifiers,
           py_modules = py_modules,
           packages = packages,
           data_files = data_files,
           url=url,
           ext_modules = ext_modules )
