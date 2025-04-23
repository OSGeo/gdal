#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Benchmarking
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
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
