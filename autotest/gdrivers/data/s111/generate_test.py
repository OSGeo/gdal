#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Generate test_s111.h5
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

import h5py
import numpy as np


def generate(filename, version):
    f = h5py.File(os.path.join(os.path.dirname(__file__), f"{filename}.h5"), "w")
    SurfaceCurrent = f.create_group("SurfaceCurrent")
    SurfaceCurrent_01 = SurfaceCurrent.create_group("SurfaceCurrent.01")
    Group_001 = SurfaceCurrent_01.create_group("Group_001")

    SurfaceCurrent.attrs["dataCodingFormat"] = np.uint8(2)
    SurfaceCurrent.attrs["minDatasetCurrentSpeed"] = np.float64(1)
    SurfaceCurrent.attrs["maxDatasetCurrentSpeed"] = np.float64(2)

    values_struct_type = np.dtype(
        [
            ("surfaceCurrentSpeed", "f4"),
            ("surfaceCurrentDirection", "f4"),
        ]
    )
    values = Group_001.create_dataset("values", (2, 3), dtype=values_struct_type)
    data = np.array(
        [(-123, 0), (1, 1), (2, 2), (3, 3), (4, 2), (5, 1)],
        dtype=values_struct_type,
    ).reshape(values.shape)
    values[...] = data

    Group_001.attrs["timePoint"] = "20190606T120000Z"

    SurfaceCurrent_01.attrs["gridOriginLongitude"] = np.float64(2)
    SurfaceCurrent_01.attrs["gridOriginLatitude"] = np.float64(48)
    SurfaceCurrent_01.attrs["gridSpacingLongitudinal"] = np.float64(0.4)
    SurfaceCurrent_01.attrs["gridSpacingLatitudinal"] = np.float64(0.5)
    SurfaceCurrent_01.attrs["numPointsLongitudinal"] = np.uint32(values.shape[1])
    SurfaceCurrent_01.attrs["numPointsLatitudinal"] = np.uint32(values.shape[0])

    SurfaceCurrent_01.attrs["numberOfTimes"] = np.uint32(1)
    SurfaceCurrent_01.attrs["timeRecordInterval"] = np.uint16(3600)
    SurfaceCurrent_01.attrs["dateTimeOfFirstRecord"] = "20190606T120000Z"
    SurfaceCurrent_01.attrs["dateTimeOfLastRecord"] = "20190606T120000Z"

    SurfaceCurrent_01.attrs["numGRP"] = np.uint32(1)
    SurfaceCurrent_01.attrs["startSequence"] = "0,0"

    Group_F = f.create_group("Group_F")
    Group_F_SurfaceCurrent_struct_type = np.dtype(
        [
            ("code", "S20"),
            ("name", "S20"),
            ("uom.name", "S20"),
            ("fillValue", "S20"),
            ("datatype", "S20"),
            ("lower", "S20"),
            ("upper", "S20"),
            ("closure", "S20"),
        ]
    )
    Group_F_SurfaceCurrent = Group_F.create_dataset(
        "SurfaceCurrent", (3,), dtype=Group_F_SurfaceCurrent_struct_type
    )
    Group_F_SurfaceCurrent[...] = np.array(
        [
            (
                "surfaceCurrentSpeed",
                "Surface Current Speed",
                "knot",
                "-123.0",
                "H5T_FLOAT",
                "0.00",
                "",
                "geSemiInterval",
            ),
            (
                "surfaceCurrentDirection",
                "Surface Current Direction",
                "degree",
                "-123.0",
                "H5T_FLOAT",
                "0.00",
                "359.9",
                "closedInterval",
            ),
            (
                "surfaceCurrentTime",
                "Surface Current Time",
                "DateTime",
                "",
                "H5T_STRING",
                "19000101T000000Z",
                "21500101T000000Z9",
                "closedInterval",
            ),
        ],
        dtype=Group_F_SurfaceCurrent_struct_type,
    )

    f.attrs["issueDate"] = "2023-12-31"
    f.attrs["geographicIdentifier"] = "Somewhere"
    f.attrs["verticalDatum"] = np.int16(12)
    f.attrs["horizontalCRS"] = np.int32(4326)
    f.attrs["verticalCS"] = np.int32(6498)  # Depth, metres, orientation down
    f.attrs["verticalCoordinateBase"] = np.int32(2)
    f.attrs["verticalDatumReference"] = np.int32(1)
    f.attrs["productSpecification"] = version
    f.attrs[
        "producer"
    ] = "Generated by autotest/gdrivers/data/s111/generate_test.py (not strictly fully S111 compliant)"
    f.attrs["metadata"] = f"MD_{filename}.xml"

    open(os.path.join(os.path.dirname(__file__), f.attrs["metadata"]), "wb").write(
        b"<nothing/>"
    )


generate("test_s111_v1.2", "INT.IHO.S-111.1.2")
