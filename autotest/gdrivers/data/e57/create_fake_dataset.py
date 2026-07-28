# -*- coding: utf-8 -*-
###############################################################################
#
# Purpose:  Generate fake E57 datasets
# Author:   Even Rouault <even.rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even.rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import os
import struct

from osgeo import gdal

gdal.UseExceptions()


def empty():

    filename = os.path.join(os.path.dirname(__file__), "empty.e57")
    with open(filename, "wb") as f:

        xml = b'<e57Root type="Structure" xmlns="http://www.astm.org/COMMIT/E57/2010-e57-v1.0"/>'
        f.write(b"ASTM-E57")
        f.write(struct.pack("<I", 1))  # version major
        f.write(struct.pack("<I", 0))  # version minor
        f.write(struct.pack("<Q", 1024))  # file physical size
        f.write(struct.pack("<Q", 48))  # xml physical offset
        f.write(struct.pack("<Q", len(xml)))  # xml logical size
        f.write(struct.pack("<Q", 1024))  # page size
        f.write(xml)
        f.write(b"\x00" * (1024 - 4 - 48 - len(xml)))
        f.write(struct.pack("<I", 571394396))  # checksum reported by pdal info
        assert f.tell() == 1024, f.tell()


def single_image():

    ds = gdal.Open(os.path.join(os.path.dirname(os.path.dirname(__file__)), "byte.tif"))
    jpeg_filename = "/vsimem/temp.jpg"
    gdal.GetDriverByName("JPEG").CreateCopy(jpeg_filename, ds)
    with gdal.VSIFile(jpeg_filename, "rb") as f:
        jpeg_blob = f.read()

    mask_filename = "/vsimem/temp.png"
    src_ds = gdal.GetDriverByName("MEM").Create("", 20, 20, 1)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 20, 10, b"\xff" * (20 * 10))
    gdal.GetDriverByName("PNG").CreateCopy(mask_filename, src_ds)
    with gdal.VSIFile(mask_filename, "rb") as f:
        mask_blob = f.read()

    filename = os.path.join(os.path.dirname(__file__), "fake.e57")
    offset_jpeg = 2048 - 4 - 16 - 4
    offset_mask = 3072
    with open(filename, "wb") as f:

        xml = f"""<e57Root type="Structure" xmlns="http://www.astm.org/COMMIT/E57/2010-e57-v1.0">
<images2D type="Vector" allowHeterogeneousChildren="1">
<vectorChild type="Structure">
<guid type="String"><![CDATA[guid]]></guid>
<name type="String"><![CDATA[image name]]></name>
<associatedData3DGuid type="String"><![CDATA[associatedData3DGuid]]></associatedData3DGuid>
<pose type="Structure">
<rotation type="Structure">
  <w type="Float">rotation.w</w>
  <x type="Float"/>
  <y type="Float"/>
  <z type="Float">rotation.z</z>
</rotation>
<translation type="Structure">
  <x type="Float">translation.x</x>
  <y type="Float">translation.y</y>
  <z type="Float">translation.z</z>
</translation>
</pose>
<sphericalRepresentation type="Structure">
<jpegImage type="Blob" fileOffset="{offset_jpeg}" length="{len(jpeg_blob)}"/>
<imageMask type="Blob" fileOffset="{offset_mask}" length="{len(mask_blob)}"/>
<imageHeight type="Integer">20</imageHeight>
<imageWidth type="Integer">20</imageWidth>
</sphericalRepresentation>
</vectorChild>
</images2D>
</e57Root>""".encode(
            "utf-8"
        )

        assert len(xml) < 1024 - 4 - 16 - 4

        f.write(b"ASTM-E57")
        f.write(struct.pack("<I", 1))  # version major
        f.write(struct.pack("<I", 0))  # version minor
        f.write(struct.pack("<Q", 3072 + 16 + len(mask_blob)))  # file physical size
        f.write(struct.pack("<Q", 1024))  # xml physical offset
        f.write(struct.pack("<Q", len(xml)))  # xml logical size
        f.write(struct.pack("<Q", 1024))  # page size
        f.write(b"\x00" * (1024 - 4 - 48))
        f.write(struct.pack("<I", 1075449787))  # checksum reported by pdal info
        assert f.tell() == 1024, f.tell()

        f.write(xml)
        f.write(b"\x00" * (1024 - 4 - len(xml) - 16 - len(jpeg_blob[0:4])))
        assert f.tell() == offset_jpeg, (f.tell(), offset_jpeg)
        f.write(b"\x00" * 8)
        f.write(struct.pack("<Q", len(jpeg_blob)))
        f.write(jpeg_blob[0:4])
        f.write(struct.pack("<I", 3660337185))  # checksum reported by pdal info
        assert f.tell() == 2048, f.tell()

        f.write(jpeg_blob[4:])
        f.write(b"\x00" * (1024 - 4 - len(jpeg_blob[4:])))
        f.write(struct.pack("<I", 0))  # (dummy) checksum
        assert f.tell() == 3072, f.tell()

        f.write(b"\x00" * 8)
        f.write(struct.pack("<Q", len(mask_blob)))
        f.write(mask_blob)


def two_images():

    ds = gdal.Open(os.path.join(os.path.dirname(os.path.dirname(__file__)), "byte.tif"))
    jpeg_filename = "/vsimem/temp.jpg"
    gdal.GetDriverByName("JPEG").CreateCopy(jpeg_filename, ds)
    with gdal.VSIFile(jpeg_filename, "rb") as f:
        jpeg_blob = f.read()
    png_filename = "/vsimem/temp.png"
    gdal.GetDriverByName("PNG").CreateCopy(png_filename, ds)
    with gdal.VSIFile(png_filename, "rb") as f:
        png_blob = f.read()
    filename = os.path.join(os.path.dirname(__file__), "fake_two_images.e57")

    offset_jpeg = 2048 - 4 - 16 - 4
    offset_png = 3072

    with open(filename, "wb") as f:

        xml = f"""<e57Root type="Structure" xmlns="http://www.astm.org/COMMIT/E57/2010-e57-v1.0">
<images2D type="Vector" allowHeterogeneousChildren="1">
<vectorChild type="Structure">
<guid type="String"><![CDATA[guid]]></guid>
<name type="String"><![CDATA[image]]></name>
<associatedData3DGuid type="String"><![CDATA[associatedData3DGuid]]></associatedData3DGuid>
<sphericalRepresentation type="Structure">
<jpegImage type="Blob" fileOffset="{offset_jpeg}" length="{len(jpeg_blob)}"/>
</sphericalRepresentation>
</vectorChild>
<vectorChild type="Structure">
<name type="String"><![CDATA[image2]]></name>
<pinholeRepresentation type="Structure">
<pngImage type="Blob" fileOffset="{offset_png}" length="{len(png_blob)}"/>
</pinholeRepresentation>
</vectorChild>
</images2D>
</e57Root>""".encode(
            "utf-8"
        )

        assert len(xml) < 1024 - 4 - 16 - 4

        f.write(b"ASTM-E57")
        f.write(struct.pack("<I", 1))  # version major
        f.write(struct.pack("<I", 0))  # version minor
        f.write(struct.pack("<Q", 3072 + 16 + len(png_blob)))  # file physical size
        f.write(struct.pack("<Q", 1024))  # xml physical offset
        f.write(struct.pack("<Q", len(xml)))  # xml logical size
        f.write(struct.pack("<Q", 1024))  # page size
        f.write(b"\x00" * (1024 - 4 - 48))
        f.write(struct.pack("<I", 1527807958))  # checksum reported by pdal info
        assert f.tell() == 1024, f.tell()

        f.write(xml)
        f.write(b"\x00" * (1024 - 4 - len(xml) - 16 - len(jpeg_blob[0:4])))
        assert f.tell() == offset_jpeg, (f.tell(), offset_jpeg)
        f.write(b"\x00" * 8)
        f.write(struct.pack("<Q", len(jpeg_blob)))
        f.write(jpeg_blob[0:4])
        f.write(struct.pack("<I", 1480594145))  # checksum reported by pdal info
        assert f.tell() == 2048, f.tell()

        f.write(jpeg_blob[4:])
        f.write(b"\x00" * (1024 - 4 - len(jpeg_blob[4:])))
        f.write(struct.pack("<I", 0))  # (dummy) checksum
        assert f.tell() == 3072, f.tell()

        f.write(b"\x00" * 8)
        f.write(struct.pack("<Q", len(png_blob)))
        f.write(png_blob)


empty()
single_image()
two_images()
