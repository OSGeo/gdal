.. _geotransforms_tut:

================================================================================
Geotransform Tutorial
================================================================================

Introduction to Geotransforms:
-------------------------------

0 = x-coordinate of the upper-left corner of the upper-left pixel
1 = w-e pixel resolution / pixel width,
2 = row rotation (typically zero)
3 = y-coordinate of the of the upper-left corner of the upper-left pixel
4 = column rotation (typically zero)
5 = n-s pixel resolution / pixel height (typically negative value)

Xgeo = gt(0) + Xpixel * gt(1) + Yline * gt(2)
Ygeo = gt(3) + Xpixel * gt(4) + Yline * gt(5)

In case of north up images:
----------------------------
(GT(2), GT(4)) coefficients are zero,
(GT(1), GT(5)) is pixel size
(GT(0), GT(3)) position is the top left corner of the top left pixel of the raster.

Note that the pixel/line coordinates in the above are from (0.0,0.0) at the top left corner of the top left pixel
to (width_in_pixels,height_in_pixels) at the bottom right corner of the bottom right pixel.
The pixel/line location of the center of the top left pixel would therefore be (0.5,0.5).
