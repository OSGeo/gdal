.. _raster.msg:

================================================================================
MSG -- Meteosat Second Generation
================================================================================

.. shortname:: MSG

.. build_dependencies:: msg library

This driver implements reading support for Meteosat Second Generation
files. These are files with names like
``H-000-MSG1\_\_-MSG1\_\_\_\_\_\_\_\_-HRV\_\_\_\_\_\_-000007\_\_\_-200405311115-C\_``, commonly
distributed into a folder structure with dates (e.g. ``2004\05\31`` for the
file mentioned).

The MSG files are wavelet-compressed. A decompression library licensed
from `EUMETSAT <http://www.eumetsat.int/>`__ is needed (`Public Wavelet
Transform Decompression Library
Software <https://gitlab.eumetsat.int/open-source/PublicDecompWT>`__,
shorter *Wavelet Transform Software*). The software is compilable on
Microsoft Windows, Linux and Solaris Operating Systems, and it works on
32 bits and 64 bits as well as mixed architectures. It is licensed
under Apache v2.

| This driver is not "enabled" by default. See `Build
  Instructions <#MSG_Build_Instructions>`__ on how to include this
  driver in your GDAL library.

Driver capabilities
-------------------

.. supports_georeferencing::

Build Instructions
------------------

Clone the EUMETSAT library for wavelet decompression into ``frmts/msg``.

If you are building with Visual Studio 6.0, extract the .vc makefiles
for the PublicDecompWT from the file `PublicDecompWTMakefiles.zip`
stored in that directory.

If you build using the GNUMakefile, use *--with-msg* option to enable
MSG driver:

::

   ./configure --with-msg

If you find that some adjustments are needed in the makefile and/or the msg
source files, please "commit" them. The EUMETSAT library promises to be
"platform independent", but as we are working with Microsoft Windows and
Visual Studio 6.0, we did not have the facilities to check if the rest
of the msg driver is. Furthermore, apply steps 4 to 7 from the :ref:`raster_driver_tut`, section "Adding
Driver to GDAL Tree".

MSG Wiki page is available at http://trac.osgeo.org/gdal/wiki/MSG. It's
dedicated to document building and usage hints

Specification of Source Dataset
-------------------------------

It is possible to select individual files for opening. In this case, the
driver will gather the files that correspond to the other strips of the
same image, and correctly compose the image.

Example with gdal_translate.exe:

::

   gdal_translate
    C:\hrit_a\2004\05\31\H-000-MSG1__-MSG1________-HRV______-000008___-200405311115-C_
    C:\output\myimage.tif

It is also possible to use the following syntax for opening the MSG
files:

-  MSG(source_folder,timestamp,(channel,channel,...,channel),use_root_folder,data_conversion,nr_cycles,step)
-

   -  source_folder: a path to a folder structure that contains the
      files
   -  timestamp: 12 digits representing a date/time that identifies the
      114 files of the 12 images of that time, e.g. 200501181200
   -  channel: a number between 1 and 12, representing each of the 12
      available channels. When only specifying one channel, the brackets
      are optional.
   -  use_root_folder: Y to indicate that the files reside directly into
      the source_folder specified. N to indicate that the files reside
      in date structured folders: source_folder/YYYY/MM/DD
   -  data_conversion:
   -

      -  N to keep the original 10 bits DN values. The result is UInt16.
      -  B to convert to 8 bits (handy for GIF and JPEG images). The
         result is Byte.
      -  R to perform radiometric calibration and get the result in
         mW/m2/sr/(cm-1)-1. The result is Float32.
      -  L to perform radiometric calibration and get the result in
         W/m2/sr/um. The result is Float32.
      -  T to get the reflectance for the visible bands (1, 2, 3 and 12)
         and the temperature in degrees Kelvin for the infrared bands
         (all other bands). The result is Float32.

   -  nr_cycles: a number that indicates the number of consecutive
      cycles to be included in the same file (time series). These are
      appended as additional bands.
   -  step: a number that indicates what is the stepsize when multiple
      cycles are chosen. E.g. every 15 minutes: step = 1, every 30
      minutes: step = 2 etc. Note that the cycles are exactly 15 minutes
      apart, so you can not get images from times in-between (the step
      is an integer).

Examples with gdal_translate utility:

Example call to fetch an MSG image of 200501181200 with bands 1, 2 and 3
in IMG format:

::

   gdal_translate -of HFA MSG(\\pc2133-24002\RawData\,200501181200,(1,2,3),N,N,1,1) d:\output\outfile.img

In JPG format, and converting the 10 bits image to 8 bits by dividing
all values by 4:

::

   gdal_translate -of JPEG MSG(\\pc2133-24002\RawData\,200501181200,(1,2,3),N,B,1,1) d:\output\outfile.jpg

The same, but reordering the bands in the JPEG image to resemble RGB:

::

   gdal_translate -of JPEG MSG(\\pc2133-24002\RawData\,200501181200,(3,2,1),N,B,1,1) d:\output\outfile.jpg

Geotiff output, only band 2, original 10 bits values:

::

   gdal_translate -of GTiff MSG(\\pc2133-24002\RawData\,200501181200,2,N,N,1,1) d:\output\outfile.tif

Band 12:

::

   gdal_translate -of GTiff MSG(\\pc2133-24002\RawData\,200501181200,12,N,N,1,1) d:\output\outfile.tif

The same band 12 with radiometric calibration in mW/m2/sr/(cm-1)-1:

::

   gdal_translate -of GTiff MSG(\\pc2133-24002\RawData\,200501181200,12,N,R,1,1) d:\output\outfile.tif

Retrieve data from c:\hrit-data\2005\01\18 instead of
\\\pc2133-24002\RawData\... :

::

   gdal_translate -of GTiff MSG(c:\hrit-data\2005\01\18,200501181200,12,Y,R,1,1) d:\output\outfile.tif

Another option to do the same (note the difference in the Y and the N
for the “use_root_folder” parameter:

::

   gdal_translate -of GTiff MSG(c:\hrit-data\,200501181200,12,N,R,1,1) d:\output\outfile.tif

Without radiometric calibration, but for 10 consecutive cycles (thus
from 1200 to 1415):

::

   gdal_translate -of GTiff MSG(c:\hrit-data\,200501181200,12,N,N,10,1) d:\output\outfile.tif

10 cycles, but every hour (thus from 1200 to 2100):

::

   gdal_translate -of GTiff MSG(c:\hrit-data\,200501181200,12,N,N,10,4) d:\output\outfile.tif

10 cycles, every hour, and bands 3, 2 and 1:

::

   gdal_translate -of GTiff MSG(c:\hrit-data\,200501181200,(3,2,1),N,N,10,4) d:\output\outfile.tif

Georeference and Projection
---------------------------

The images are using the Geostationary Satellite View projection. Most
GIS packages don't recognize this projection (we only know of ILWIS that
does have this projection), but gdalwarp.exe can be used to re-project
the images.

See Also
--------

-  Implemented as ``gdal/frmts/msg/msgdataset.cpp``.
-  http://www.eumetsat.int - European Organisation for the Exploitation
   of Meteorological Satellites
