.. _raster.nitf_advanced:

================================================================================
NITF -- Advanced Driver Information
================================================================================

The NITF (National Imagery Transmission Format) driver in GDAL includes
a number of advanced, and somewhat esoteric options not suitable for the
:ref:`general end user documentation <raster.nitf>` for the driver. This
information is collected here, and is primarily aimed at developers and
advanced users.

CGM Segments
------------

NITF files that have CGM data (that is segment type GR - graphics, or SY
with an STYPE value of 'C') will make that information available as
metadata in the CGM domain. The returned metadata will look something
like:

::

     SEGMENT_COUNT=1
     SEGMENT_0_SLOC_ROW=25
     SEGMENT_0_SLOC_COL=25
     SEGMENT_0_SDLVL=2
     SEGMENT_0_SALVL=1
     SEGMENT_0_CCS_ROW=00025
     SEGMENT_0_CCS_COL=00025
     SEGMENT_0_DATA=\0!\0...

The SLOC_ROW and SLOC_COL values are the placement of the CGM object
relative to the base (SALVL) image. The CCS_ROW/COL values are relative
to the common coordinate system. The \_SDLVL is the display level. The
DATA is the raw CGM data with "backslash quotable" escaping applied. All
occurrences of ASCII zero will be translated to '\0', and all
backslashes and double quotes will be backslashed escaped. The
CPLUnescapeString() function can be used to unescape the data into
binary format using scheme CPLES_BackslashQuotable.

To add CGM data to a NITF image, you can pass creation
options in the following format:

::

     CGM=SEGMENT_COUNT=1
     CGM=SEGMENT_0_SLOC_ROW=25
     CGM=SEGMENT_0_SLOC_COL=25
     CGM=SEGMENT_0_SDLVL=2
     CGM=SEGMENT_0_SALVL=1
     CGM=SEGMENT_0_DATA=\0!\0...

Notice that passing CGM as creation options will overwrite existing CGM
segment read in the CGM metadata domain.

While GDAL does not support parsing or rendering CGM data, at least one
user has found the
`UniConverter <http://sk1project.org/modules.php?name=Products&product=uniconvertor>`__
library useful for this purpose.

Multi-Image NITF Files
----------------------

NITF files with more than one image segment (IM) will present the image
segments as subdatasets. Opening a multiple NITF file by filename will
provide access to the first image segment. The subdataset metadata for a
3 image NITF file might look like:

::

   Subdatasets:
     SUBDATASET_1_NAME=NITF_IM:0:multi_image_jpeg_2.0.ntf
     SUBDATASET_1_DESC=Image 1 of multi_image_jpeg_2.0.ntf
     SUBDATASET_2_NAME=NITF_IM:1:multi_image_jpeg_2.0.ntf
     SUBDATASET_2_DESC=Image 2 of multi_image_jpeg_2.0.ntf
     SUBDATASET_3_NAME=NITF_IM:2:multi_image_jpeg_2.0.ntf
     SUBDATASET_3_DESC=Image 3 of multi_image_jpeg_2.0.ntf

In this case opening "multi_image_jpeg_2.0.ntf" directly will give
access to "NITF_IM:0:multi_image_jpeg_2.0.ntf". To open the others use
the corresponding subdataset names. The Subdataset mechanism is generic
GDAL concept discussed in the :ref:`raster_data_model` document.

Text Segments
-------------

NITF files that have text segments (that is segment type TX) will make
that information available as metadata in the TEXT domain. The returned
metadata will look something like:

::

     HEADER_0=TE       00020021216151629xxxxxxxxxxxxxxxxxxxxxxxxxxx
     DATA_0=This is test text file 01.

     HEADER_1=TE       00020021216151629xxxxxxxxxxxxxxxxxxxxxxxxxxx
     DATA_1=This is test text file 02.

     HEADER_2=TE       00020021216151629xxxxxxxxxxxxxxxxxxxxxxxxxxx
     DATA_2=This is test text file 03.

     HEADER_3=TE       00020021216151629xxxxxxxxxxxxxxxxxxxxxxxxxxx
     DATA_3=This is test text file 04.

     HEADER_4=TE       00020021216151629xxxxxxxxxxxxxxxxxxxxxxxxxxx
     DATA_4=This is test text file 05.

The argument to DATA_n is the raw text of the n'th (zero based) text
segment with no escaping of any kind applied.

The TEXT segment header data is preserved in HEADER_n
metadata item. The CreateCopy() method on the NITF driver also supports
creating text segments on the output file as long as the input file has
metadata in the TEXT domain as defined above.

To add TEXT data to a NITF image, you can also pass
creation options in the following format:

::

     TEXT=HEADER_0=TE       00020021216151629xxxxxxxxxxxxxxxxxxxxxxxxxxx
     TEXT=DATA_0=This is test text file 01.
     TEXT=HEADER_1=TE       00020021216151629xxxxxxxxxxxxxxxxxxxxxxxxxxx
     TEXT=DATA_1=This is test text file 02.

Notice that passing TEXT as creation options will overwrite existing
text segment read in the TEXT metadata domain.

TREs
----

NITF files with registered (or unregistered?) extensions on the file
header, or the referenced image header will make them available in a raw
form in metadata via the TRE domain. The TRE domain will hold one
metadata item per TRE which will have the name of the TRE as the name,
and the data of the TRE as the contents. The data contents will be
"backslash escaped" like CGM data above.

In case of multiple occurrences of the same TRE, the second occurrence
will be named "TRENAME_2", the third "TRENAME_3" where TRENAME is the
TRE name.

::

   Metadata (TRE):
     GEOPSB=MAPM  World Geodetic System 1984
                  WGE World Geodetic System 1984
                      WE Geodetic
                         GEODMean Sea
                             MSL 000000000000000
                                                   0000
     PRJPSB=Polar Stereographic
            PG2-00090.00000250000039.99999884000000000000000000000000000000
     MAPLOB=M  0598005958-000003067885.8-000002163353.8

TREs as xml:TRE
---------------

All TREs found in file and matching one of the
TRE description of the
`nitf_spec.xml <http://trac.osgeo.org/gdal/browser/trunk/gdal/data/nitf_spec.xml>`__
in GDAL data directory will be reported as XML content in the xml:TRE
metadata domain.

::

   Metadata (xml:TRE):
   <tres>
     <tre name="RSMDCA" location="des TRE_OVERFLOW">
       <field name="IID" value="2_8" />
       <field name="EDITION" value="1101222272-2" />
       <field name="TID" value="1101222272-1" />
       <field name="NPAR" value="06" />
       <field name="NIMGE" value="001" />
       <field name="NPART" value="00006" />
       <repeated name="IMAGE" number="1">
         <group index="0">
           <field name="IID" value="2_8" />
           <field name="NPARI" value="06" />
         </group>
       </repeated>
       <field name="XUOL" value="-2.42965895449297E+06" />
       <field name="YUOL" value="-4.76049894293300E+06" />
       <field name="ZUOL" value="+3.46898407315533E+06" />
       <field name="XUXL" value="+8.90698769551156E-01" />
       <field name="XUYL" value="+2.48664813021570E-01" />
       <field name="XUZL" value="-3.80554217799520E-01" />
       <field name="YUXL" value="-4.54593996792805E-01" />
       <field name="YUYL" value="+4.87215943350720E-01" />
       <field name="YUZL" value="-7.45630553709282E-01" />
       <field name="ZUXL" value="+0.00000000000000E+00" />
       <field name="ZUYL" value="+8.37129879594448E-01" />
       <field name="ZUZL" value="+5.47004172461403E-01" />
   [...]
       <repeated name="DERCOV" number="21">
         <group index="0">
           <field name="DERCOV" value="+5.77388827727787E+04" />
         </group>
   [...]
         <group index="20">
           <field name="DERCOV" value="+1.14369570920252E-02" />
         </group>
       </repeated>
     </tre>
     <tre name="RSMECA" location="des TRE_OVERFLOW">
   [...]
     </tre>
     <tre name="RSMIDA" location="des TRE_OVERFLOW">
   [...]
     </tre>
     <tre name="RSMPCA" location="des TRE_OVERFLOW">
   [...]
     </tre>
   </tres>

TRE creation from hexadecimal data
----------------------------------

TRE data can be added to a newly created NITF file in hexadecimal format to encode binary
data such as unsigned int or floating point types.  The hexadecimal TRE creation option is
supplied as "TRE=HEX/<tre_name>=<hex_tre_data>" or "FILE_TRE=HEX/<tre_name>=<hex_tre_data>

.. code-block:: python

    # Encode "ABC" as 3 bytes of hex data, "414243"
    ds = gdal.GetDriverByName('NITF').Create('/vsimem/file.ntf', 1, 1, options=["TRE=HEX/TSTTRE=414243"])

Data Extension Segments (xml:DES)
---------------------------------
Data Extension Segments (DESs) are user-defined metadata extensions to the NITF format.
The DES metadata is available through the xml\:DES metadata domain.  The xml\:DES domain
returns an XML string with standard and user-defined DES fields in plain text, and
the user-defined DES data as Base64 encoded text.  The following is an example XML structure:

::

    <des_list>
      <des name="TEST">
        <field name="NITF_DESVER" value="02" />
        <field name="NITF_DECLAS" value="U" />
        <field name="NITF_DESCLSY" value="" />
        [...]
        <field name="NITF_DESCTLN" value="" />
        <field name="NITF_DESSHL" value="0004" />
        <field name="NITF_DESSHF" value="ABCD" />
        <field name="NITF_DESDATA" value="MTIzNDU2Nzg5MA==" />
      </des>
    </des_list>

Raw File / Image Headers
------------------------

In some cases application may need to recover very specific information
from the image or file headers that isn't normally available as
metadata. In this case it is possible to query the "NITF_METADATA"
metadata domain. The complete file and image headers will be returned as
metadata in base64 encoded format. Something like:

::

   Metadata (NITF_METADATA):
     NITFFileHeader=002213 TklURjAyLjAwMDEgICAgVTIxN0cwSjA...
     NITFImageSubheader=439 SU1NaXNzaW5nIElEMjUxNTI1NTlaTU...

Note that the ascii encoded numeric values prefixing the base64 encoded
header is the length (decoded) in bytes, followed by one space.

Writing multiple Image Segments
-------------------------------

(GDAL >= 3.4)

Writing a NITF file with several image segments is done with the ``APPEND_SUBDATASET=YES``
creation option but requires some care.

The file must be created with the content of the first image segment with the ``NUMI``
creation option to specify the total number of image segments and optionally with
the ``NUMDES`` creation option with the number of DE segments.
TREs that apply to the whole file should also be specified at that stage.
The ``APPEND_SUBDATASET`` creation option must NOT be specified.

Subsequent images are appended with the ``APPEND_SUBDATASET=YES`` creation option.
The ``IDLVL``, ``IALVL``, ``ILOCROW`` and ``ILOCCOL`` creation option may be specified.

On the last image segment, the DES content, if any must be specified with the ``DES``
creation option.

.. note:: The file will only be valid if all NUMI images have been actually written.

Example:

::

    gdal_translate first_image.tif  dest.tif -co NUMI=3 -co NUMDES=1
    gdal_translate second_image.tif dest.tif -co APPEND_SUBDATASET=YES -co IC=C3 -co IDLVL=2
    gdal_translate third_image.tif  dest.tif -co APPEND_SUBDATASET=YES -co IC=C8 -co IDLVL=3 -co "DES=DES1={des_content}"
