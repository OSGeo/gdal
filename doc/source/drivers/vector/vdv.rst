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

Creation issues
---------------

The driver can create new layers (either in the same file, or in
separate files in the same directory). It can append a new layer into an
existing file, but it cannot append/edit/delete features to an existing
layer, or modify the attribute structure of an existing layer after
features have been written.

|about-dataset-creation-options|
The following dataset creation options are available:

-  .. dsco:: SINGLE_FILE
      :choices: YES, NO
      :default: YES

      Whether several layers should be put in the
      same file. If NO, the name is assumed to be a directory name.

|about-layer-creation-options|
The following layer creation options are available:

-  .. lco:: EXTENSION
      :default: x10

      Extension used when creating files in
      separate layers, i.e. only for :dsco:`SINGLE_FILE=NO` dataset creation
      option.

-  .. lco:: PROFILE
      :choices: GENERIC, VDV-452, VDV-452-ENGLISH, VDV-452-GERMAN
      :default: GENERIC

      Describe which profile the writer should conform
      to. VDV-452 will restrict layer and field names to be the one allowed
      by the VDV-452 standard (either in English or German).
      VDV-452-ENGLISH and VDV-452-GERMAN will restrict the VDV-452 to the
      specified language. The configuration file describing VDV-452 table
      and field names is
      :source_file:`ogr/ogrsf_frmts/vdv/data/vdv452.xml`
      located in the GDAL_DATA directory.

-  .. lco:: PROFILE_STRICT
      :choices: YES, NO
      :default: NO

      Whether checks of profile should be
      strict. In strict mode, unexpected layer or field names will be
      rejected.

-  .. lco:: CREATE_ALL_FIELDS
      :choices: YES, NO
      :default: YES

      Whether all fields of predefined profiles should be created at layer creation.

-  .. lco:: STANDARD_HEADER
      :choices: YES, NO
      :default: YES

      Whether to write standard header fields
      (i.e mod, src, chs, ver, ifv, dve, fft). If set to NO, only
      explicitly specified HEADER_xxx fields will be written.

-  .. lco:: HEADER_SRC
      :default: UNKNOWN

      Value of the src header field.

-  .. lco:: HEADER_SRC_DATE
      :choices: <DD.MM.YYYY>
      :default: current date (in GMT)


      Value of the date of the src header field as DD.MM.YYYY.

-  .. lco:: HEADER_SRC_TIME
      :choices: <HH.MM.SS>
      :default: current time (in GMT)

      Value of the time of the src header field as HH.MM.SS.

-  .. lco:: HEADER_CHS
      :default: ISO8859-1

      Value of the chs header field.

-  .. lco:: HEADER_VER
      :default: 1.4

      Value of the ver header field.

-  .. lco:: HEADER_IFV
      :default: 1.4

      Value of the ifv header field.

-  .. lco:: HEADER_DVE
      :default: 1.4

      Value of the dve header field.

-  .. lco:: HEADER_FFT
      :default: '' (empty string)

      Value of the fft header field.

-  .. lco:: HEADER_xxx**

      Value of the *xxx* (user defined) header field.

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
