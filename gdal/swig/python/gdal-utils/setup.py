#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Setup script for gdal-utils.

from setuptools import setup, find_packages

__pacakge_name__ = 'gdal-utils'
__version__ = '3.3.0'
__author__ = "Frank Warmerdam"
__author_email__ = "warmerdam@pobox.com"
__maintainer__ = "Idan Miara"
__maintainer_email__ = "idan@miara.com"
__description__ = "gdal-utils: An extension library for GDAL - Geospatial Data Abstraction Library"
__license_type__ = "MIT"
__url__ = "http://www.gdal.org"

classifiers = [
    'Development Status :: 5 - Production/Stable',
    'Intended Audience :: Developers',
    'Intended Audience :: Science/Research',
    'License :: OSI Approved :: MIT License',
    'Operating System :: OS Independent',
    'Programming Language :: Python :: 3',
    'Topic :: Scientific/Engineering :: GIS',
    'Topic :: Scientific/Engineering :: Information Analysis',
]

__readme__ = open('README.rst', encoding="utf-8").read()
__readme_type__ = 'text/x-rst'

package_root = '.'   # package sources are under this dir
packages = find_packages(package_root)  # include all packages under package_root
package_dir = {'': package_root}  # packages sources are under package_root

setup(
    name=__pacakge_name__,
    version=__version__,
    author=__author__,
    author_email=__author_email__,
    maintainer=__maintainer__,
    maintainer_email=__maintainer_email__,
    long_description=__readme__,
    long_description_content_type=__readme_type__,
    description=__description__,
    license=__license_type__,
    classifiers=classifiers,
    url=__url__,
    python_requires='>=3.6.0',
    packages=packages,
    package_dir=package_dir,
    install_requires=['gdal'],
    extras_require={'numpy': ['numpy > 1.0.0']},
)

