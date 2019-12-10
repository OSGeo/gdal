#!/usr/bin/env python
# -*- coding: utf-8 -*-
# This code is in the public domain, so as to serve as a template for
# real-world plugins.
# or, at the choice of the licensee,
# Copyright 2019 Even Rouault
# SPDX-License-Identifier: MIT

from gdal_python_driver import BaseDriver

# gdal: DRIVER_NAME = "UNSUPPORTED_API_VERSION"
# API version(s) supported. Must include 1 currently
# gdal: DRIVER_SUPPORTED_API_VERSION = [2]
# gdal: DRIVER_DCAP_VECTOR = "YES"
# gdal: DRIVER_DMD_LONGNAME = "my super plugin"

class Driver(BaseDriver):
    def __init__(self):
        pass
