# SPDX-License-Identifier: MIT
# Copyright 2023 Even Rouault

# This benchmark utility helps assessing the efficiency of the bypass
# of the block cache when writing GeoTIFF datasets.

import array
import sys
from threading import Thread

from osgeo import gdal

gdal.UseExceptions()


def Usage():
    print("gtiff_multi_ds_parallel_write.py [--nbands VAL] [--without-optim]")
    print("                                 [--compress NONE/ZSTD/...]")
    print("                                 [--buffer-interleave PIXEL/BAND]")
    sys.exit(1)


num_threads = gdal.GetNumCPUs()
nbands = 1
with_optim = True
compression = "ZSTD"
buffer_pixel_interleaved = True
width = 2048
height = 2048

# Parse arguments
i = 1
while i < len(sys.argv):
    if sys.argv[i] == "--nbands":
        i += 1
        nbands = int(sys.argv[i])
    elif sys.argv[i] == "--compress":
        i += 1
        compression = sys.argv[i]
    elif sys.argv[i] == "--buffer-interleave":
        i += 1
        buffer_pixel_interleaved = sys.argv[i] == "PIXEL"
    elif sys.argv[i] == "--without-optim":
        with_optim = False
    else:
        Usage()
    i += 1

nloops = 1000 // nbands
data = array.array("B", [i % 255 for i in range(nbands * width * height)])

gdal.SetCacheMax(width * height * nbands * num_threads)


def thread_function(num):
    filename = "/vsimem/tmp%d.tif" % num
    drv = gdal.GetDriverByName("GTiff")
    options = ["TILED=YES", "COMPRESS=" + compression]
    for i in range(nloops):
        ds = drv.Create(filename, width, height, nbands, options=options)
        if not with_optim:
            # Calling ReadRaster() disables the cache bypass write optimization
            ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1)
        if nbands > 1:
            if buffer_pixel_interleaved:
                # Write pixel-interleaved buffer for maximum efficiency
                ds.WriteRaster(
                    0,
                    0,
                    width,
                    height,
                    data,
                    buf_pixel_space=nbands,
                    buf_line_space=width * nbands,
                    buf_band_space=1,
                )
            else:
                ds.WriteRaster(0, 0, width, height, data)
        else:
            ds.GetRasterBand(1).WriteRaster(0, 0, width, height, data)


# Spawn num_threads running thread_function
threads_array = []

for i in range(num_threads):
    t = Thread(target=thread_function, args=[i])
    t.start()
    threads_array.append(t)

for t in threads_array:
    t.join()
