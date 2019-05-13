.. _vector.segy:

SEG-Y / SEGY
============

(GDAL/OGR >= 1.9.0)

This driver reads files in SEG-Y format. Those files are binary files
that contain single-line seismic digital data. The driver will report
the attributes of the trace header (in their raw form, see the SEG-Y
specification for more information), and use the receiver group
coordinates as geometry. The sample values are also reported.

A layer "{basefilename}_header" is also created and contains a single
feature with the content of the text and binary file headers.

See Also
~~~~~~~~

-  `Description of SEG-Y format
   Rev1 <http://www.seg.org/documents/10161/77915/seg_y_rev1.pdf>`__
-  `Wikipedia page about SEG-Y
   format <http://en.wikipedia.org/wiki/SEG_Y>`__
