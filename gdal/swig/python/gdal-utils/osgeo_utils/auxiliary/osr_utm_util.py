#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL osgeo_utils.auxiliary
#  Purpose:  OSR UTM utility functions
#  Author:   Idan Miara <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2021, Idan Miara <idan@miara.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************

import math
from numbers import Real

from typing import List, Tuple, Optional

from osgeo_utils.auxiliary.base import  Real2D
from osgeo_utils.auxiliary.osr_util import AnySRS, get_srs


def get_utm_zone_center(float_zone: Real) -> Real:
    zone_center = (float_zone - 30.5) * 6  # == (i-30.5)*6*pi/180
    if zone_center <= -180:
        zone_center += 360
    elif zone_center > 180:
        zone_center -= 360
    return zone_center


def get_utm_zone_by_lon(lon_degrees: Real, allow_float_zone: bool = False) -> Real:
    if allow_float_zone:
        return lon_degrees / 6 + 30.5
    else:
        zone = math.floor(lon_degrees / 6) + 31
        if zone > 60:
            zone = zone - 60  # Zones 1 - 30
        return zone


def get_datum_and_zone_from_srs(srs: AnySRS) -> Tuple[str, Real]:
    srs = get_srs(srs)
    datum = srs.GetAttrValue('DATUM')
    central_meridian = srs.GetProjParm('central_meridian')
    if central_meridian:
        zone = get_utm_zone_by_lon(central_meridian)
    else:
        zone = None
    return datum, zone


def get_utm_zone_extent_points(float_zone: Real, width=10) -> List[Real2D]:
    zone_center = get_utm_zone_center(float_zone)
    x_arr = [zone_center - width / 2.0, zone_center + width / 2.0]
    y_arr = [-80, 80]

    extent_points = []
    for x in x_arr:
        for y in y_arr:
            extent_points.append((x, y))
        y_arr.reverse()
    return extent_points


def proj_string_from_utm_zone(zone: Optional[Real] = None, datum_str='+datum=WGS84') -> str:
    is_geo = not zone
    if is_geo:
        pj_string = '+proj=latlong'
    elif float(zone).is_integer():
        pj_string = f'+proj=utm +zone={int(zone)}'
    else:
        pj_string = f'+proj=tmerc +k=0.9996 +lon_0={get_utm_zone_center(zone)} +x_0=500000'
    pj_string = pj_string + ' ' + datum_str
    if not is_geo:
        pj_string = pj_string + ' +units=m'
    pj_string = pj_string + ' +no_defs'

    return pj_string


