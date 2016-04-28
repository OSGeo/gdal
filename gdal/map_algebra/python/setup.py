#!/usr/bin/env python

from distutils.core import setup, Extension

module = Extension('_gma',
                   sources=['gma_wrap.cxx'],
                   include_dirs=['..','/home/ajolma/usr/include'],
                   library_dirs=['..','/home/ajolma/usr/lib'],
                   runtime_library_dirs=['..','/home/ajolma/usr/lib'],
                   libraries=['gma','gdal'],
)

setup (name = 'gma',
       version = '0.1',
       author      = "Ari Jolma",
       description = """GDAL Map Algebra""",
       ext_modules = [module],
       py_modules = ["gma"],
)
