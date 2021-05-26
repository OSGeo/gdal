#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Setup script for GDAL Python bindings.
# Inspired by psycopg2 setup.py file
# http://www.initd.org/tracker/psycopg/browser/psycopg2/trunk/setup.py
# Howard Butler hobu.inc@gmail.com


gdal_version = '3.3.0'

import sys
import os

from glob import glob
from distutils.sysconfig import get_config_vars
from distutils.command.build_ext import build_ext
from distutils.ccompiler import get_default_compiler
from distutils.errors import CompileError

# Strip -Wstrict-prototypes from compiler options, if present. This is
# not required when compiling a C++ extension.
(opt,) = get_config_vars('OPT')
if opt is not None:
    os.environ['OPT'] = " ".join(f for f in opt.split() if f != '-Wstrict-prototypes')

# If CXX is defined in the environment, it will be used to link the .so
# but distutils will be confused if it is made of several words like 'ccache g++'
# and it will try to use only the first word.
# See https://lists.osgeo.org/pipermail/gdal-dev/2016-July/044686.html
# Note: in general when doing "make", CXX will not be defined, unless it is defined as
# an environment variable, but in that case it is the value of GDALmake.opt that
# will be set, not the one from the environment that started "make" !
# If no CXX environment variable is defined, then the value of the CXX variable
# in GDALmake.opt will not be set as an environment variable
if 'CXX' in os.environ and os.environ['CXX'].strip().find(' ') >= 0:
    if os.environ['CXX'].strip().startswith('ccache ') and os.environ['CXX'].strip()[len('ccache '):].find(' ') < 0:
        os.environ['CXX'] = os.environ['CXX'].strip()[len('ccache '):]
    else:
        print('WARNING: "CXX=%s" was defined in the environment and contains more than one word. Unsetting it since that is incompatible of distutils' % os.environ['CXX'])
        del os.environ['CXX']
if 'CC' in os.environ and os.environ['CC'].strip().find(' ') >= 0:
    if os.environ['CC'].strip().startswith('ccache ') and os.environ['CC'].strip()[len('ccache '):].find(' ') < 0:
        os.environ['CC'] = os.environ['CC'].strip()[len('ccache '):]
    else:
        print('WARNING: "CC=%s" was defined in the environment and contains more than one word. Unsetting it since that is incompatible of distutils' % os.environ['CC'])
        del os.environ['CC']

# ---------------------------------------------------------------------------
# Switches
# ---------------------------------------------------------------------------

HAVE_NUMPY = False
HAVE_SETUPTOOLS = False
BUILD_FOR_CHEESESHOP = False
GNM_ENABLED = True

# ---------------------------------------------------------------------------
# Default build options
# (may be overridden with setup.cfg or command line switches).
# ---------------------------------------------------------------------------

include_dirs = ['../../port', '../../gcore', '../../alg', '../../ogr/', '../../ogr/ogrsf_frmts', '../../gnm', '../../apps']
library_dirs = ['../../.libs', '../../']
libraries = ['gdal']


# ---------------------------------------------------------------------------
# Helper Functions
# ---------------------------------------------------------------------------

# Function to find numpy's include directory
def get_numpy_include():
    if HAVE_NUMPY:
        return numpy.get_include()
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
        #  print ('numpy include', get_numpy_include())
        if get_numpy_include() == '.':
            print("WARNING: numpy headers were not found!  Array support will not be enabled")
            HAVE_NUMPY = False
except ImportError:
    print('WARNING: numpy not available!  Array support will not be enabled')
    pass

try:
    from setuptools import setup, find_packages
    from setuptools import Extension
    HAVE_SETUPTOOLS = True
except ImportError:
    from distutils.core import setup, Extension
    from distutils.command.build_py import build_py
    from distutils.command.build_scripts import build_scripts


class gdal_config_error(Exception):
    pass


def fetch_config(option, gdal_config='gdal-config'):

    command = gdal_config + " --%s" % option

    import subprocess
    command, args = command.split()[0], command.split()[1]
    try:
        p = subprocess.Popen([command, args], stdout=subprocess.PIPE)
    except OSError:
        e = sys.exc_info()[1]
        raise gdal_config_error(e)
    r = p.stdout.readline().decode('ascii').strip()
    p.stdout.close()
    p.wait()

    return r


def supports_cxx11(compiler, compiler_flag=None):
    ret = False
    with open('gdal_python_cxx11_test.cpp', 'wt') as f:
        f.write("""
#if __cplusplus < 201103L
#error "C++11 required"
#endif
int main () { return 0; }""")
        f.close()
        extra_postargs = None
        if compiler_flag:
            extra_postargs = [compiler_flag]

        if os.name == 'posix':
            # Redirect stderr to /dev/null to hide any error messages
            # from the compiler.
            devnull = open(os.devnull, 'w')
            oldstderr = os.dup(sys.stderr.fileno())
            os.dup2(devnull.fileno(), sys.stderr.fileno())
            try:
                compiler.compile([f.name], extra_postargs=extra_postargs)
                ret = True
            except CompileError:
                pass
            os.dup2(oldstderr, sys.stderr.fileno())
            devnull.close()
        else:
            try:
                compiler.compile([f.name], extra_postargs=extra_postargs)
                ret = True
            except CompileError:
                pass
    os.unlink('gdal_python_cxx11_test.cpp')
    if os.path.exists('gdal_python_cxx11_test.o'):
        os.unlink('gdal_python_cxx11_test.o')
    return ret

###Based on: https://stackoverflow.com/questions/28641408/how-to-tell-which-compiler-will-be-invoked-for-a-python-c-extension-in-setuptool
def has_flag(compiler, flagname):
    import tempfile
    from distutils.errors import CompileError
    with tempfile.NamedTemporaryFile('w', suffix='.cpp') as f:
        f.write('int main (int argc, char **argv) { return 0; }')
        try:
            compiler.compile([f.name], extra_postargs=[flagname])
        except CompileError:
            return False
    return True


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
        self.extra_cflags = []
        self.parallel = True # Python 3.5 only

    def get_compiler(self):
        return self.compiler or get_default_compiler()

    def get_gdal_config(self, option):
        try:
            return fetch_config(option, gdal_config=self.gdal_config)
        except gdal_config_error:
            # If an error is thrown, it is possibly because
            # the gdal-config location given in setup.cfg is
            # incorrect, or possibly the default -- ../../apps/gdal-config
            # We'll try to use the gdal-config that might be on the path.
            try:
                return fetch_config(option)
            except gdal_config_error as e:
                msg = 'Could not find gdal-config. Make sure you have installed the GDAL native library and development headers.'
                import sys
                import traceback
                traceback_string = ''.join(traceback.format_exception(*sys.exc_info()))
                raise gdal_config_error(traceback_string + '\n' + msg)


    def build_extensions(self):

        # Add a -std=c++11 or similar flag if needed
        ct = self.compiler.compiler_type
        if ct == 'unix' and not supports_cxx11(self.compiler):
            cxx11_flag = None
            if supports_cxx11(self.compiler, '-std=c++11'):
                cxx11_flag = '-std=c++11'
            if cxx11_flag:
                for ext in self.extensions:
                    # gdalconst builds as a .c file
                    if ext.name != 'osgeo._gdalconst':
                        ext.extra_compile_args += [cxx11_flag]

                    # Adding arch flags here if OS X and compiler is clang
                    if sys.platform == 'darwin' and [int(x) for x in os.uname()[2].split('.')] >= [11, 0, 0]:
                        # since MacOS X 10.9, clang no longer accepts -mno-fused-madd
                        # extra_compile_args.append('-Qunused-arguments')
                        clang_flag = '-Wno-error=unused-command-line-argument-hard-error-in-future'
                        if has_flag(self.compiler, clang_flag):
                            ext.extra_compile_args += [clang_flag]
                        else:
                            clang_flag = '-Wno-error=unused-command-line-argument'
                            if has_flag(self.compiler, clang_flag):
                                ext.extra_compile_args += [clang_flag]

        build_ext.build_extensions(self)

    def finalize_options(self):
        global include_dirs, library_dirs

        if self.include_dirs is None:
            self.include_dirs = include_dirs
        # Needed on recent MacOSX
        elif isinstance(self.include_dirs, str) and sys.platform == 'darwin':
            self.include_dirs += ':' + ':'.join(include_dirs)
        if self.library_dirs is None:
            self.library_dirs = library_dirs
        # Needed on recent MacOSX
        elif isinstance(self.library_dirs, str) and sys.platform == 'darwin':
            self.library_dirs += ':' + ':'.join(library_dirs)
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
        self.library_dirs.append(os.path.join(self.gdaldir, 'lib'))
        self.include_dirs.append(os.path.join(self.gdaldir, 'include'))

        cflags = self.get_gdal_config('cflags')
        if cflags:
            self.extra_cflags = cflags.split()

    def build_extension(self, ext):
        # We override this instead of setting extra_compile_args directly on
        # the Extension() instantiations below because we want to use the same
        # logic to resolve the location of gdal-config throughout.
        ext.extra_compile_args.extend(self.extra_cflags)
        return build_ext.build_extension(self, ext)


extra_link_args = []
extra_compile_args = []

gdal_module = Extension('osgeo._gdal',
                        sources=['extensions/gdal_wrap.cpp'],
                        extra_compile_args=extra_compile_args,
                        extra_link_args=extra_link_args)

gdalconst_module = Extension('osgeo._gdalconst',
                             sources=['extensions/gdalconst_wrap.c'],
                             extra_compile_args=extra_compile_args,
                             extra_link_args=extra_link_args)

osr_module = Extension('osgeo._osr',
                       sources=['extensions/osr_wrap.cpp'],
                       extra_compile_args=extra_compile_args,
                       extra_link_args=extra_link_args)

ogr_module = Extension('osgeo._ogr',
                       sources=['extensions/ogr_wrap.cpp'],
                       extra_compile_args=extra_compile_args,
                       extra_link_args=extra_link_args)


array_module = Extension('osgeo._gdal_array',
                         sources=['extensions/gdal_array_wrap.cpp'],
                         extra_compile_args=extra_compile_args,
                         extra_link_args=extra_link_args)

gnm_module = Extension('osgeo._gnm',
                       sources=['extensions/gnm_wrap.cpp'],
                       extra_compile_args=extra_compile_args,
                       extra_link_args=extra_link_args)

ext_modules = [gdal_module,
               gdalconst_module,
               osr_module,
               ogr_module]

if os.path.exists('setup_vars.ini'):
    with open('setup_vars.ini') as f:
        lines = f.readlines()
        if 'GNM_ENABLED=no' in lines or 'GNM_ENABLED=no\n' in lines:
            GNM_ENABLED = False

if GNM_ENABLED:
    ext_modules.append(gnm_module)

if HAVE_NUMPY:
    ext_modules.append(array_module)

utils_package_root = 'gdal-utils'   # path for gdal-utils sources
if HAVE_SETUPTOOLS:
    packages = find_packages(utils_package_root)
else:
    packages = ['osgeo_utils', 'osgeo_utils.auxiliary', 'osgeo_utils.samples']
packages = ['osgeo'] + packages
package_dir = {'osgeo': 'osgeo', '': utils_package_root}

readme = open('README.rst', encoding="utf-8").read()

name = 'GDAL'
version = gdal_version
author = "Frank Warmerdam"
author_email = "warmerdam@pobox.com"
maintainer = "Howard Butler"
maintainer_email = "hobu.inc@gmail.com"
description = "GDAL: Geospatial Data Abstraction Library"
license_type = "MIT"
url = "http://www.gdal.org"

classifiers = [
    'Development Status :: 5 - Production/Stable',
    'Intended Audience :: Developers',
    'Intended Audience :: Science/Research',
    'License :: OSI Approved :: MIT License',
    'Operating System :: OS Independent',
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

exclude_package_data = {'': ['GNUmakefile']}

setup_kwargs = dict(
    name=name,
    version=gdal_version,
    author=author,
    author_email=author_email,
    maintainer=maintainer,
    maintainer_email=maintainer_email,
    long_description=readme,
    long_description_content_type='text/x-rst',
    description=description,
    license=license_type,
    classifiers=classifiers,
    packages=packages,
    package_dir=package_dir,
    url=url,
    python_requires='>=3.6.0',
    data_files=data_files,
    ext_modules=ext_modules,
    scripts=glob(utils_package_root + '/scripts/*.py'),
    cmdclass={'build_ext': gdal_ext},
    extras_require={'numpy': ['numpy > 1.0.0']},
)

if HAVE_SETUPTOOLS:
    setup_kwargs['zip_safe'] = False
    setup_kwargs['exclude_package_data'] = exclude_package_data
else:
    setup_kwargs['cmdclass']['build_py'] = build_py
    setup_kwargs['cmdclass']['build_scripts'] = build_scripts

setup(**setup_kwargs)
