#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Setup script for gdal-utils.
from glob import glob

from setuptools import setup, find_packages

from osgeo_utils import (
    __pacakge_name__,
    __version__,
    __author__,
    __author_email__,
    __maintainer__,
    __maintainer_email__,
    __description__,
    __license_type__,
    __url__,
)

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
    scripts=glob('./scripts/*.py'),
    install_requires=['gdal'],
    extras_require={'numpy': ['numpy > 1.0.0']},
)
