#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Benchmarking
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import os

from osgeo import gdal


def pytest_report_header(config):
    gdal_header_info = ""

    if os.path.exists("/sys/devices/system/cpu/intel_pstate/no_turbo"):
        content = open("/sys/devices/system/cpu/intel_pstate/no_turbo", "rb").read()
        if content[0] == b"0"[0]:
            gdal_header_info += "\n"
            gdal_header_info += "WARNING WARNING\n"
            gdal_header_info += "---------------\n"
            gdal_header_info += "Intel TurboBoost is enabled. Benchmarking results will not be accurate.\n"
            gdal_header_info += "Disable TurboBoost with: 'echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo'\n"
            gdal_header_info += "---------------\n"
            gdal_header_info += "WARNING WARNING\n"

    if "debug" in gdal.VersionInfo(""):
        gdal_header_info += "WARNING: Running benchmarks on debug build. Results will not be accurate.\n"
    gdal_header_info += "Usable RAM: %d MB\n" % (
        gdal.GetUsablePhysicalRAM() / (1024 * 1024)
    )
    gdal_header_info += "Number of CPUs: %d\n" % (gdal.GetNumCPUs())

    return gdal_header_info
