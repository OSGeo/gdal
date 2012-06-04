#!/usr/bin/env python
 
# Setup script for GDAL Python bindings.
# Inspired by psycopg2 setup.py file
# http://www.initd.org/tracker/psycopg/browser/psycopg2/trunk/setup.py
# Howard Butler hobu.inc@gmail.com


gdal_version = '1.9.0'

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

# ---------------------------------------------------------------------------
# Default build options
# (may be overriden with setup.cfg or command line switches).
# ---------------------------------------------------------------------------

include_dirs = ['../../port', '../../gcore', '../../alg', '../../ogr/']
library_dirs = ['../../.libs', '../../']
libraries = ['gdal']

# ---------------------------------------------------------------------------
# Helper Functions
# ---------------------------------------------------------------------------

# Function to find numpy's include directory
def get_numpy_include():
    if HAVE_NUMPY:
        return numpy.get_include()
    else:
        return '.'

# ---------------------------------------------------------------------------
# Imports
# ---------------------------------------------------------------------------

try:
    import numpy
    HAVE_NUMPY = True
    # check version
    numpy_major = numpy.__version__.split('.')[0]
    if int(numpy_major) < 1:
        print("numpy version must be > 1.0.0")
        HAVE_NUMPY = False
    else:
#        print ('numpy include', get_numpy_include())
        if get_numpy_include() =='.':
            print("numpy headers were not found!  Array support will not be enabled")
            HAVE_NUMPY=False
except ImportError:
    pass

fixer_names = [
    'import',
    'next',
    'renames',
    'unicode',
    'ws_comma',
    'xrange',
]
extra = {}
try:
    from setuptools import setup
    from setuptools import Extension
    HAVE_SETUPTOOLS = True
except ImportError:
    from distutils.core import setup, Extension

    try:
        from distutils.command.build_py import build_py_2to3 as build_py
        from distutils.command.build_py import build_scripts_2to3 as build_scripts
    except ImportError:
        from distutils.command.build_py import build_py
        from distutils.command.build_py import build_scripts
    else:
        build_py.fixer_names = fixer_names
        build_scripts.fixer_names = fixer_names
else:
    if sys.version_info >= (3,):
        from lib2to3.refactor import get_fixers_from_package

        all_fixers = set(get_fixers_from_package('lib2to3.fixes'))
        fixers = ['lib2to3.fixes.fix_' + name for name in fixer_names]
        exclude_fixers = sorted(all_fixers.difference(fixers))

        extra['use_2to3'] = True
        extra['use_2to3_fixers'] = []
        extra['use_2to3_exclude_fixers'] = exclude_fixers

class gdal_config_error(Exception): pass


from distutils.command.build_ext import build_ext
from distutils.ccompiler import get_default_compiler
from distutils.sysconfig import get_python_inc

def fetch_config(option, gdal_config='gdal-config'):

    command = gdal_config + " --%s" % option

    try:
        import subprocess
        command, args = command.split()[0], command.split()[1]
        from sys import version_info
        if version_info >= (3,0,0):
            try:
                p = subprocess.Popen([command, args], stdout=subprocess.PIPE)
            except OSError(e):
                raise gdal_config_error(e)
            r = p.stdout.readline().decode('ascii').strip()
        else:
            exec("""try:
    p = subprocess.Popen([command, args], stdout=subprocess.PIPE)
except OSError, e:
    raise gdal_config_error, e""")
            r = p.stdout.readline().strip()
        p.stdout.close()
        p.wait()

    except ImportError:
        
        import popen2
        
        p = popen2.popen3(command)
        r = p[0].readline().strip()
        if not r:
            raise Warning(p[2].readline())
    
    return r
    
class gdal_ext(build_ext):

    GDAL_CONFIG = 'gdal-config'
    user_options = build_ext.user_options[:]
    user_options.extend([
        ('gdal-config=', None,
        "The name of the gdal-config binary and/or a full path to it"),
    ])

    def initialize_options(self):
        build_ext.initialize_options(self)

        self.numpy_include_dir = get_numpy_include()
        self.gdaldir = None
        self.gdal_config = self.GDAL_CONFIG
        self.already_raised_no_config_error = False

    def get_compiler(self):
        return self.compiler or get_default_compiler()
    
    def get_gdal_config(self, option):
        try:
            return fetch_config(option, gdal_config = self.gdal_config)
        except gdal_config_error:
            # If an error is thrown, it is possibly because 
            # the gdal-config location given in setup.cfg is 
            # incorrect, or possibly the default -- ../../apps/gdal-config
            # We'll try one time to use the gdal-config that might be 
            # on the path. If that fails, we're done, however.
            if not self.already_raised_no_config_error:
                self.already_raised_no_config_error = True
                return fetch_config(option)
            
    def finalize_options(self):
        if self.include_dirs is None:
            self.include_dirs = include_dirs
        if self.library_dirs is None:
            self.library_dirs = library_dirs
        if self.libraries is None:
            if self.get_compiler() == 'msvc':
                libraries.remove('gdal')
                libraries.append('gdal_i')
            self.libraries = libraries

        build_ext.finalize_options(self)
        
        self.include_dirs.append(self.numpy_include_dir)
        
        if self.get_compiler() == 'msvc':
            return True

        self.gdaldir = self.get_gdal_config('prefix')
        self.library_dirs.append(os.path.join(self.gdaldir,'lib'))
        self.include_dirs.append(os.path.join(self.gdaldir,'include'))


extra_link_args = []
extra_compile_args = []

gdal_module = Extension('osgeo._gdal',
                        sources=['extensions/gdal_wrap.cpp'],
                        extra_compile_args = extra_compile_args,
                        extra_link_args = extra_link_args)

gdalconst_module = Extension('osgeo._gdalconst',
                    sources=['extensions/gdalconst_wrap.c'],
                    extra_compile_args = extra_compile_args,
                    extra_link_args = extra_link_args)

osr_module = Extension('osgeo._osr',
                    sources=['extensions/osr_wrap.cpp'],
                    extra_compile_args = extra_compile_args,
                    extra_link_args = extra_link_args)

ogr_module = Extension('osgeo._ogr',
                    sources=['extensions/ogr_wrap.cpp'],
                    extra_compile_args = extra_compile_args,
                    extra_link_args = extra_link_args)


array_module = Extension('osgeo._gdal_array',
                    sources=['extensions/gdal_array_wrap.cpp'],
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

readme = str(open('README.txt','rb').read())

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
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 3',
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
           cmdclass={'build_ext':gdal_ext},
           ext_modules = ext_modules,
           **extra )
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
           cmdclass={'build_ext':gdal_ext,
                     'build_py': build_py,
                     'build_scripts': build_scripts},
           ext_modules = ext_modules )
