#!/bin/sh

for codec in NONE LZW LZMA ZSTD DEFLATE JPEG LERC LERC_DEFLATE LERC_ZSTD JXL
do
  gdal_translate ../byte.tif byte_${codec}.tif -co COMPRESS=${codec}
  gdal_translate ../byte.tif byte_${codec}_tiled.tif -co COMPRESS=${codec} -co TILED=YES -co BLOCKXSIZE=16 -co BLOCKYSIZE=16
done

gdal_translate ../byte.tif byte_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2
gdal_translate ../byte.tif uint16_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt16
gdal_translate ../byte.tif uint32_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt32
gdal_translate ../byte.tif uint64_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt64
gdal_translate ../byte.tif float32_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot Float32
gdal_translate ../byte.tif float64_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot Float64
gdal_translate ../byte.tif float32_LZW_predictor_3.tif -co COMPRESS=LZW -co PREDICTOR=3 -ot Float32
gdal_translate ../byte.tif float64_LZW_predictor_3.tif -co COMPRESS=LZW -co PREDICTOR=3 -ot Float64

for codec in NONE LZW LZMA ZSTD DEFLATE JPEG LERC LERC_DEFLATE LERC_ZSTD JXL WEBP
do
  gdal_translate ../rgbsmall.tif rgbsmall_${codec}.tif -co COMPRESS=${codec} -co INTERLEAVE=PIXEL -co WEBP_LOSSLESS=YES
  gdal_translate ../rgbsmall.tif rgbsmall_${codec}_tiled.tif -co COMPRESS=${codec} -co TILED=YES -co BLOCKXSIZE=16 -co BLOCKYSIZE=32 -co INTERLEAVE=PIXEL -co WEBP_LOSSLESS=YES
  if test "${codec}" != "WEBP"; then
    gdal_translate ../rgbsmall.tif rgbsmall_${codec}_separate.tif -co COMPRESS=${codec} -co INTERLEAVE=BAND
    gdal_translate ../rgbsmall.tif rgbsmall_${codec}_tiled_separate.tif -co COMPRESS=${codec} -co TILED=YES -co BLOCKXSIZE=16 -co BLOCKYSIZE=32 -co INTERLEAVE=BAND
  fi
done

gdal_translate ../rgbsmall.tif rgbsmall_JPEG_ycbcr.tif -co COMPRESS=JPEG -co PHOTOMETRIC=YCBCR -co INTERLEAVE=PIXEL

gdal_translate ../rgbsmall.tif rgbsmall_byte_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2
gdal_translate ../rgbsmall.tif rgbsmall_uint16_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt16
gdal_translate ../rgbsmall.tif rgbsmall_uint32_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt32
gdal_translate ../rgbsmall.tif rgbsmall_uint64_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt64

gdal_translate ../stefan_full_rgba.tif stefan_full_rgba_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2

gdal_translate ../stefan_full_greyalpha.tif stefan_full_greyalpha_byte_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2
gdal_translate ../stefan_full_greyalpha.tif stefan_full_greyalpha_uint16_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt16
gdal_translate ../stefan_full_greyalpha.tif stefan_full_greyalpha_uint32_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt32
gdal_translate ../stefan_full_greyalpha.tif stefan_full_greyalpha_uint64_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -ot UInt64

gdal_translate ../byte.tif byte_5_bands_LZW_predictor_2.tif -co COMPRESS=LZW -co PREDICTOR=2 -b 1 -b 1 -b 1 -b 1 -b 1 -scale_2 0 255 255 0 -scale_4 0 255 255 0
