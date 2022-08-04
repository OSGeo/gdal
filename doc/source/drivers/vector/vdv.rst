.. _vector.vdv:

VDV - VDV-451/VDV-452/INTREST Data Format
=========================================

.. versionadded:: 2.1

.. shortname:: VDV

.. built_in_by_default::

This driver can read and create text files following the VDV-451 file
format, which is a text format similar to CSV files, potentially
containing several layers within the same file.

It supports in particular reading 2 "profiles" :

-  (read/write) VDV-452 standard for route network / timetable
-  (read/only) "INTREST Data format" used by the `Austrian official open
   government street
   graph <https://www.data.gv.at/katalog/dataset/3fefc838-791d-4dde-975b-a4131a54e7c5>`__

The generic reader/writer for VDV-451/VDV-452 can support arbitrarily
large files. For the INTREST data case, for combined layers in a single
file, the driver ingests the whole file in memory to reconstruct the
Link layer.

Interleave reading among layers is supported in files with multiple
layers.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Creations issues
----------------

The driver can create new layers (either in the same file, or in
separate files in the same directory). It can append a new layer into an
existing file, but it cannot append/edit/delete features to an existing
layer, or modify the attribute structure of an existing layer after
features have been written.

The following dataset creation options are available:

-  **SINGLE_FILE**\ =YES/NO. Whether several layers should be put in the
   same file. If NO, the name is assumed to be a directory name.
   Defaults to YES.

The following layer creation options are available:

-  **EXTENSION**\ =string. Extension used when creation files in
   separate layers, i.e. only for SINGLE_FILE=NO dataset creation
   option. Defaults to x10.
-  **PROFILE**\ =GENERIC/VDV-452/VDV-452-ENGLISH/VDV-452-GERMAN.
   Defaults to GENERIC. Describe which profile the writer should conform
   to. VDV-452 will restrict layer and field names to be the one allowed
   by the VDV-452 standard (either in English or German).
   VDV-452-ENGLISH and VDV-452-GERMAN will restrict the VDV-452 to the
   specified language. The configuration file describing VDV-452 table
   and field names is
   `vdv452.xml <https://github.com/OSGeo/gdal/blob/master/data/vdv452.xml>`__
   located in the GDAL_DATA directory.
-  **PROFILE_STRICT**\ =YES/NO. Whether checks of profile should be
   strict. In strict mode, unexpected layer or field names will be
   rejected. Defaults to NO.
-  **CREATE_ALL_FIELDS**\ =YES/NO. Whether all fields of predefined
   profiles should be created at layer creation. Defaults to YES.
-  **STANDARD_HEADER**\ =YES/NO. Whether to write standard header fields
   (i.e mod, src, chs, ver, ifv, dve, fft). If set to NO, only
   explicitly specified HEADER_xxx fields will be written. Defaults to
   YES.
-  **HEADER_SRC**\ =string: Value of the src header field. Defaults to
   UNKNOWN.
-  **HEADER_SRC_DATE**\ =string: Value of the date of the src header
   field as DD.MM.YYYY. Defaults to current date (in GMT).
-  **HEADER_SRC_TIME**\ =string: Value of the time of the src header
   field as HH.MM.SS. Defaults to current time (in GMT)
-  **HEADER_CHS**\ =string: Value of the chs header field. Defaults to
   ISO8859-1.
-  **HEADER_VER**\ =string: Value of the ver header field. Defaults to
   1.4.
-  **HEADER_IFV**\ =string: Value of the ifv header field. Defaults to
   1.4.
-  **HEADER_DVE**\ =string: Value of the dve header field. Defaults to
   1.4.
-  **HEADER_FFT**\ =string: Value of the fft header field. Defaults to
   '' (empty string).
-  **HEADER\_**\ *xxx*\ =string: Value of the *xxx* (user defined)
   header field.

Links
-----

-  `VDV-451 file
   format <https://www.vdv.de/vdv-schrift-451.pdfx?forced=false>`__
   (German)
-  `VDV-452 data
   model <https://www.vdv.de/service/downloads_onp.aspx?id=4328&forced=false>`__ (German)
-  `Austrian INTREST data
   format <https://gip.gv.at/assets/downloads/1908_dokumentation_gipat_ogd.pdf>`__
   (German)
