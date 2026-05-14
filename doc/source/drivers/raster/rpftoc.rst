.. _raster.rpftoc:

================================================================================
RPFTOC -- Raster Product Format/RPF (a.toc)
================================================================================

.. shortname:: RPFTOC

.. built_in_by_default::

This is a reader for RPF products, like CADRG or CIB, that
uses the table of content file - A.TOC - from a RPF exchange, and
exposes it as a virtual dataset whose coverage is the set of frames
contained in the table of content.

Starting with GDAL 3.13, the A.TOC file can also be generated using the
``gdal driver rpftoc create`` program from existing CADRG frames.

The driver will report a different subdataset for each subdataset found
in the A.TOC file.

Result of a gdalinfo on a A.TOC file.

::

   Subdatasets:
     SUBDATASET_1_NAME=NITF_TOC_ENTRY:CADRG_GNC_5M_1_1:GNCJNCN/rpf/a.toc
     SUBDATASET_1_DESC=CADRG:GNC:Global Navigation Chart:5M:1:1
   [...]
     SUBDATASET_5_NAME=NITF_TOC_ENTRY:CADRG_GNC_5M_7_5:GNCJNCN/rpf/a.toc
     SUBDATASET_5_DESC=CADRG:GNC:Global Navigation Chart:5M:7:5
     SUBDATASET_6_NAME=NITF_TOC_ENTRY:CADRG_JNC_2M_1_6:GNCJNCN/rpf/a.toc
     SUBDATASET_6_DESC=CADRG:JNC:Jet Navigation Chart:2M:1:6
   [...]
     SUBDATASET_13_NAME=NITF_TOC_ENTRY:CADRG_JNC_2M_8_13:GNCJNCN/rpf/a.toc
     SUBDATASET_13_DESC=CADRG:JNC:Jet Navigation Chart:2M:8:13

It is possible to build external overviews for a subdataset. The
overview for the first subdataset will be named A.TOC.1.ovr for example,
for the second dataset it will be A.TOC.2.ovr, etc. Note that you must
re-open the subdataset with the same setting of :config:`RPFTOC_FORCE_RGBA` as the
one you have used when you have created it. Do not use any method other
than NEAREST resampling when building overviews on a paletted subdataset
(:config:`RPFTOC_FORCE_RGBA` unset)

A gdalinfo on one of this subdataset will return the various NITF
metadata, as well as the list of the NITF tiles of the subdataset.

See Also:

-  `MIL-PRF-89038 <http://www.everyspec.com/MIL-PRF/MIL-PRF+%28080000+-+99999%29/MIL-PRF-89038_25371/>`__
   : specification of RPF, CADRG, CIB products

NOTE: Implemented as :source_file:`frmts/nitf/rpftocdataset.cpp`

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

|about-open-options|
This driver supports the following open options:

- .. oo:: FORCE_RGBA
     :choices: YES, NO
     :default: NO
     :since: 3.13

     In some situations, :ref:`raster.nitf` tiles inside a subdataset
     don't share the same palettes. The RPFTOC driver will do its best to
     remap palettes to the reported palette by gdalinfo (which is the palette
     of the first tile of the subdataset). If it doesn't produce the desired
     result, you can set this option to YES to get a RGBA dataset, instead of a
     paletted one.

     Equivalent to setting the :config:`RPFTOC_FORCE_RGBA` configuration option.

Configuration options
---------------------

|about-config-options|
This paragraph lists the configuration options that can be set to alter
the default behavior of the RPFTC driver.

-  .. config:: RPFTOC_FORCE_RGBA
     :choices: YES, NO
     :default: NO

      Equivalent to setting the :oo:`FORCE_RGBA` open option.

.. _raster.rpftoc.create:

Creation of a A.TOC file from existing CADRG frames
---------------------------------------------------

.. versionadded:: 3.13

Description
++++++++++++

:program:`gdal driver rpftoc create` can be used to create a A.TOC file from
existing CADRG frames. The value of the ``input`` argument should be the
path to the ``RPF`` directory under which the CADRG frames are found. The
``A.TOC`` file will be written in that directory.

Synopsis
++++++++

.. program-output:: gdal driver rpftoc create --help-doc


Program-Specific Options
++++++++++++++++++++++++

.. option:: --scale <SCALE>

   (Reciprocal) scale (e.g. 1000000). If not specified, it will be guessed from
   the content of CADRG frames (except for those where this cannot be inferred
   automatically)

.. option:: --producer-id <PRODUCER-ID>

   Producer (short) identification. Up to 5 characters.

.. option:: --producer-name <PRODUCER-NAME>

   Producer name. Up to 10 characters.

.. option:: --contry-code <CONTRY-CODE>

   Two letter ISO country code for security classification

.. option:: --classification U|R|C|S|T

   Index classification. Defaults to U (Unclassified)

Examples
++++++++

::

    gdal driver rpftoc create /path/to/RPF --producer-id NIMA
