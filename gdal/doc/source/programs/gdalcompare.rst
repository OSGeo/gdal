.. raw:: html

   <div id="top">

.. raw:: html

   <div id="titlearea">

+--------------------------------------------------------------------------+
| .. raw:: html                                                            |
|                                                                          |
|    <div id="projectname">                                                |
|                                                                          |
| GDAL                                                                     |
|                                                                          |
| .. raw:: html                                                            |
|                                                                          |
|    </div>                                                                |
+--------------------------------------------------------------------------+

.. raw:: html

   </div>

.. raw:: html

   <div id="navrow1" class="tabs">

-  `Main Page <https://www.gdal.org/index.html>`__
-  `Related Pages <https://www.gdal.org/pages.html>`__
-  `Classes <https://www.gdal.org/annotated.html>`__
-  `Files <https://www.gdal.org/files.html>`__
-  `Download <https://www.gdal.org/usergroup0.html>`__
-  `Issue Tracker <https://github.com/OSGeo/gdal/issues/>`__

.. raw:: html

   </div>

.. raw:: html

   </div>

.. raw:: html

   <div class="header">

.. raw:: html

   <div class="headertitle">

.. raw:: html

   <div class="title">

gdalcompare.py

.. raw:: html

   </div>

.. raw:: html

   </div>

.. raw:: html

   </div>

.. raw:: html

   <div class="contents">

.. raw:: html

   <div class="textblock">

Compare two images.

.. rubric::  SYNOPSIS
   :name: synopsis

.. code:: fragment

    gdalcompare.py [-sds] golden_file new_file

.. rubric::  DESCRIPTION
   :name: description

The gdalcompare.py script compares two GDAL supported datasets and
reports the differences. In addition to reporting differences to the
standard out the script will also return the difference count in it's
exit value.

Image pixels, and various metadata are checked. There is also a byte by
byte comparison done which will count as one difference. So if it is
only important that the GDAL visible data is identical a difference
count of 1 (the binary difference) should be considered acceptable.

**-sds**:
    If this flag is passed the script will compare all subdatasets that
    are part of the dataset, otherwise subdatasets are ignored.

*golden\_file*:
    The file that is considered correct, referred to as the golden file.

*new\_file*:
    The file being compared to the golden file, referred to as the new
    file.

Note that the gdalcompare.py script can also be called as a library from
python code though it is not typically in the python path for including.
The primary entry point is gdalcompare.compare() which takes a golden
gdal.Dataset and a new gdal.Dataset as arguments and returns a
difference count (excluding the binary comparison). The
gdalcompare.compare\_sds() entry point can be used to compare
subdatasets.

.. raw:: html

   </div>

.. raw:: html

   </div>

--------------

Generated for GDAL by |doxygen| 1.8.8.

.. |doxygen| image:: ./GDAL_%20gdalcompare.py_files/doxygen.png
   :target: http://www.doxygen.org/index.html
