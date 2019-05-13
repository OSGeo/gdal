.. _vector.segukooa:

SEG-P1 / UKOOA P1/90
====================

(GDAL/OGR >= 1.9.0)

This driver reads files in SEG-P1 and UKOOA P1/90 formats. Those files
are simple ASCII files that contain seismic shotpoints. Two layers are
reported : one with the points, and another ones where sequential points
with same line name are merged together in a single feature with a line
geometry.

See Also
~~~~~~~~

-  `Description of SEG-P1
   format <http://www.seg.org/documents/10161/77915/seg_p1_p2_p3.pdf>`__
-  `Description of UKOOA P1/90
   format <https://www.iogp.org/wp-content/uploads/2016/12/P1.pdf>`__
