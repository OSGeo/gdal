#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ******************************************************************************
#
#  Project:  GDAL
#  Purpose:  rectangle class (x, y, width, height)
#  Author:   Idan Miara, <idan@miara.com>
#
# ******************************************************************************
#  Copyright (c) 2020, Idan Miara <idan@miara.com>
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


class GeoRectangle:
    __slots__ = ['x', 'y', 'w', 'h']

    def __init__(self, x, y, w, h, allow_negative_size=False):
        if w <= 0:
            if allow_negative_size:
                x = x + w
                w = -w
            else:
                w = 0
        if h <= 0:
            if allow_negative_size:
                y = y + h
                h = -h
            else:
                h = 0
        self.x = x
        self.y = y
        self.w = w
        self.h = h

    def __eq__(self, other):
        if not isinstance(other, GeoRectangle):
            # don't attempt to compare against unrelated types
            return False
        return self.xywh == other.xywh

    def __round__(self, *args, **kwargs):
        return self.from_lrdu(*(round(i, *args, **kwargs) for i in self.lrdu))

    def is_empty(self):
        return self.w <= 0 or self.h <= 0

    def intersect(self, other: "GeoRectangle"):
        return GeoRectangle.from_min_max(
            max(self.min_x, other.min_x),
            min(self.max_x, other.max_x),
            max(self.min_y, other.min_y),
            min(self.max_y, other.max_y),
        )

    def union(self, other: "GeoRectangle"):
        return GeoRectangle.from_min_max(
            min(self.min_x, other.min_x),
            max(self.max_x, other.max_x),
            min(self.min_y, other.min_y),
            max(self.max_y, other.max_y),
        )

    def round(self, digits):
        self.x = round(self.x, digits)
        self.y = round(self.y, digits)
        self.w = round(self.w, digits)
        self.h = round(self.h, digits)

    def align(self, geo_transform):
        # compute the pixel-aligned bounding box (larger than the feature's bbox)
        left = self.min_x - (self.min_x - geo_transform[0]) % geo_transform[1]
        right = self.max_x + (geo_transform[1] - ((self.max_x - geo_transform[0]) % geo_transform[1]))
        bottom = self.min_y + (geo_transform[5] - ((self.min_y - geo_transform[3]) % geo_transform[5]))
        top = self.max_y - (self.max_y - geo_transform[3]) % geo_transform[5]
        return self.from_lrud(left, right, top, bottom)

    def get_partition(self, part: "GeoRectangle"):
        # part: x,y - part indexes; w,h - part counts
        part_width = self.w / part.w
        part_hight = self.h / part.h
        return GeoRectangle(
            self.x + part.x * part_width,
            self.y + part.y * part_hight,
            part_width,
            part_hight,
        )

    @classmethod
    def empty(cls):
        return cls(0, 0, 0, 0)

    @classmethod
    def from_lrud(cls, l, r, u, d):
        ret = cls(l, d, r - l, u - d)
        return ret

    @classmethod
    # same as min_max
    def from_lrdu(cls, l, r, d, u):
        ret = cls(l, d, r - l, u - d)
        return ret

    @classmethod
    def from_lurd(cls, l, u, r, d):
        """  from projwin (minx maxy maxx miny) == (ulx uly lrx lry) == (l u r d) """
        ret = cls(l, d, r - l, u - d)
        return ret

    @classmethod
    # same as min_max
    def from_xwyh(cls, x, w, y, h, allow_negative_size=False):
        ret = cls(x, y, w, h, allow_negative_size)
        return ret

    @classmethod
    # # same as cls
    def from_xywh(cls, x, y, w, h, allow_negative_size=False):
        ret = cls(x, y, w, h, allow_negative_size)
        return ret

    @classmethod
    # # same as cls
    def from_xywhps(cls, x, y, w, h, px, py):
        ret = cls(x, y, w*px, h*py, True)
        return ret

    @classmethod
    # same as lrdu
    def from_min_max(cls, min_x, max_x, min_y, max_y):
        ret = cls(min_x, min_y, max_x - min_x, max_y - min_y)
        return ret

    @classmethod
    def from_center_and_radius(cls, cent_x, cent_y, rad_x, rad_y=None):
        if rad_y is None:
            rad_y = rad_x
        x = cent_x - rad_x
        y = cent_y - rad_y
        w = rad_x * 2
        h = rad_y * 2
        ret = cls(x, y, w, h)
        return ret

    @classmethod
    def from_points(cls, points):
        return cls.from_min_max(
            min(p[0] for p in points),
            max(p[0] for p in points),
            min(p[1] for p in points),
            max(p[1] for p in points),
        )

    @classmethod
    def from_geotransform_and_size(cls, gt, size):
        if gt[2] or gt[4]:
            return cls.from_points(get_points_extent(gt, *size))
        else:
            # faster method
            origin = (gt[0], gt[3])
            pixel_size = (gt[1], gt[5])
            extent = cls.from_xywhps(*origin, *size, *pixel_size)
            # extent_b = cls.from_points(get_points_extent(gt, *size))
            # assert extent == extent_b
            return extent

    def to_pixels(self, pixel_size):
        return self.from_xwyh(self.x / pixel_size[0], self.w / pixel_size[0],
                              self.y / pixel_size[1], self.h / pixel_size[1], True)

    @classmethod
    def from_geotransform_and_size_to_pix(cls, gt, size):
        origin = (gt[0], gt[3])
        pixel_size = (gt[1], gt[5])
        pix_origin = list(origin[i] / pixel_size[i] for i in (0, 1))
        # pix_bounds = list(origin[i] / pixel_size[i] + size[i] for i in (0, 1))
        return cls.from_xwyh(pix_origin[0], size[0], pix_origin[1], size[1])

    @property
    def area(self):
        return self.w * self.h

    @property
    def size(self):
        return self.w, self.h

    @property
    def left(self):
        return self.x

    @property
    def right(self):
        return self.x + self.w

    @property
    def down(self):
        return self.y

    @property
    def up(self):
        return self.y + self.h

    @property
    def min_x(self):
        return self.x

    @property
    def max_x(self):
        return self.x + self.w

    @property
    def min_y(self):
        return self.y

    @property
    def max_y(self):
        return self.y + self.h

    @property
    def lurd(self):
        return self.left, self.up, self.right, self.down

    @property
    def lrud(self):
        return self.left, self.right, self.up, self.down

    @property
    def ldru(self):
        return self.left, self.down, self.right, self.up

    @property
    def lrdu(self):
        return self.left, self.right, self.down, self.up

    @property
    def xywh(self):
        return self.x, self.y, self.w, self.h

    @property
    def xwyh(self):
        return self.x, self.w, self.y, self.h

    @property
    def min_max(self):
        return self.min_x, self.max_x, self.min_y, self.max_y

    def __str__(self):
        return f"Rectangle(x[{self.min_x},{self.max_x}], y[{self.min_y},{self.max_y}] wh[{self.w},{self.h}])"

    def __repr__(self):
        return f"Rectangle(x:{self.x}, y:{self.y}, w:{self.w}, h:{self.h})"

    def __hash__(self):
        return hash(self.xywh)


def get_points_extent(gt, cols, rows):
    """Return list of corner coordinates from a geotransform"""

    def transform_point(px, py):
        x = gt[0] + (px * gt[1]) + (py * gt[2])
        y = gt[3] + (px * gt[4]) + (py * gt[5])
        return x, y

    return [
        transform_point(0, 0),
        transform_point(0, rows),
        transform_point(cols, rows),
        transform_point(cols, 0),
    ]

