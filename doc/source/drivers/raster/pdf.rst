.. _raster.pdf:

================================================================================
PDF -- Geospatial PDF
================================================================================

.. shortname:: PDF

.. build_dependencies:: none for write support, Poppler/PoDoFo/PDFium for read support

GDAL supports reading Geospatial PDF documents, by extracting
georeferencing information and rasterizing the data. Non-geospatial PDF
documents will also be recognized by the driver.

PDF documents can be created from other
GDAL raster datasets, and OGR datasources can also optionally be drawn
on top of the raster layer (see OGR\_\* creation options in the below
section).

The driver supports reading georeferencing encoded in either of the 2
current existing ways : according to the Adobe Supplement to ISO 32000,
or to a so-called OGC encoding "best practice" (which is not recommended at all).

Multipage documents are exposed as subdatasets, one subdataset par page
of the document.

Driver capabilities
-------------------

.. supports_createcopy::

.. supports_georeferencing::

.. supports_virtualio::

Vector support
--------------

See the :ref:`PDF vector <vector.pdf>` documentation page

Metadata
--------

The neatline (for OGC best practice) or the bounding box (Adobe style)
will be reported as a NEATLINE metadata item, so that it can be later
used as a cutline for the warping algorithm.

XMP metadata can be extracted from the file,
and will be stored as XML raw content in the xml:XMP metadata domain.

Additional metadata, such as found in USGS
Topo PDF can be extracted from the file, and will be stored as XML raw
content in the EMBEDDED_METADATA metadata domain.

Configuration options
---------------------

|about-config-options|
The following configuration options are supported:

-  .. config:: GDAL_PDF_DPI
      :default: 150

      To control the dimensions of the raster by
      specifying the DPI of the rasterization.
      The driver will make some effort to
      guess the DPI value either from a specific metadata item contained in
      some PDF files, or from the raster images inside the PDF (in simple
      cases).

-  .. config:: GDAL_PDF_NEATLINE

      The name of the neatline to
      select (only available for geospatial PDF, encoded according to OGC
      Best Practice). This defaults to "Map Layers" for USGS Topo PDF. If
      not found, the neatline that covers the largest area.

-  .. config:: PDF_USER_PWD

      User password for protected PDFs.

-  .. config:: GDAL_PDF_RENDERING_OPTIONS
      :choices: RASTER\,VECTOR\,TEXT, RASTER\,VECTOR, RASTER\,TEXT, RASTER, VECTOR\,TEXT, VECTOR, TEXT

      a combination of VECTOR, RASTER and
      TEXT separated by comma, to select whether vector, raster or text
      features should be rendered. If the option is not specified, all
      features are rendered (Poppler and PDFium).

-  .. config:: GDAL_PDF_BANDS
      :choices: 3, 4

      whether the PDF should be rendered as a
      RGB (3) or RGBA (4) image. The default value will depend on the PDF rendering
      used (Poppler vs PDFium) and on the content found in the PDF file (if an
      image with transparency is recognized, then 4 will be used). When 3 bands
      is selected, a white background is used.

-  .. config:: GDAL_PDF_LAYERS

      list of layers (comma separated) to turn ON (or
      "ALL" to turn all layers ON). The layer names can be obtained by
      querying the LAYERS metadata domain. When this option is specified,
      layers not explicitly listed will be turned off (Poppler and PDFium).

-  .. config:: GDAL_PDF_LAYERS_OFF

      list of layers (comma separated) to turn OFF.
      The layer names can be obtained by querying the LAYERS metadata
      domain (Poppler and PDFium).

-  .. config:: GDAL_PDF_LAUNDER_LAYER_NAMES
      :choices: YES, NO
      :since: 3.1

      Can be set to NO
      to avoid the layer names reported in the LAYERS metadata domain or as OGR
      layers for the vector part to be "laundered".

Open Options
~~~~~~~~~~~~

|about-open-options|
Above configuration options are also available as open options.

-  .. oo:: RENDERING_OPTIONS
      :choices: RASTER\,VECTOR\,TEXT, RASTER\,VECTOR, RASTER\,TEXT, RASTER, VECTOR\,TEXT, VECTOR, TEXT

      Equivalent of :config:`GDAL_PDF_RENDERING_OPTIONS` configuration option.

-  .. oo:: DPI

      Equivalent of :config:`GDAL_PDF_DPI` configuration option

-  .. oo:: USER_PWD

      Equivalent of :config:`PDF_USER_PWD` configuration option

-  .. oo:: PDF_LIB
      :choices: POPPLER, PODOFO, PDFIUM

      only available for builds with multiple backends.

-  .. oo:: LAYERS

      Equivalent of :config:`GDAL_PDF_LAYERS` configuration option

-  .. oo:: LAYERS_OFF

      Equivalent of :config:`GDAL_PDF_LAYERS_OFF` configuration option

-  .. oo:: BANDS

      Equivalent of :config:`GDAL_PDF_BANDS` configuration option

-  .. oo:: NEATLINE

      Equivalent of as :config:`GDAL_PDF_NEATLINE` configuration option

LAYERS Metadata domain
----------------------

When GDAL is compiled against Poppler
or PDFium, the LAYERS metadata domain can be queried to retrieve layer
names that can be turned ON or OFF. This is useful to know which values
to specify for the :config:`GDAL_PDF_LAYERS` or :config:`GDAL_PDF_LAYERS_OFF`
configuration options.

For example :

::

   $ gdalinfo ../autotest/gdrivers/data/adobe_style_geospatial.pdf -mdd LAYERS

   Driver: PDF/Geospatial PDF
   Files: ../autotest/gdrivers/data/adobe_style_geospatial.pdf
   [...]
   Metadata (LAYERS):
     LAYER_00_NAME=New_Data_Frame
     LAYER_01_NAME=New_Data_Frame.Graticule
     LAYER_02_NAME=Layers
     LAYER_03_NAME=Layers.Measured_Grid
     LAYER_04_NAME=Layers.Graticule
   [...]

   $ gdal_translate ../autotest/gdrivers/data/adobe_style_geospatial.pdf out.tif --config GDAL_PDF_LAYERS_OFF "New_Data_Frame"

Restrictions
------------

The opening of a PDF document (to get the georeferencing) is fast, but
at the first access to a raster block, the whole page will be rasterized
(with Poppler), which can be a slow operation.

Note: some raster-only PDF files (such as some
USGS PDF files), that are regularly tiled are exposed as tiled
dataset by the GDAL PDF driver, and can be rendered with any backends.

Only a few of the possible Datums available in the OGC best practice
spec have been currently mapped in the driver. Unrecognized datums will
be considered as being based on the WGS84 ellipsoid.

For documents that contain several neatlines in a page (insets), the
georeferencing will be extracted from the inset that has the largest
area (in term of screen points).

Creation Issues
---------------

PDF documents can be created from other GDAL raster datasets, that have
1 band (graylevel or with color table), 3 bands (RGB) or 4 bands (RGBA).

Georeferencing information will be written by default according to the
ISO32000 specification.

Note: PDF write support does not require linking to any backend.

Creation Options
~~~~~~~~~~~~~~~~

|about-creation-options|
The following creation options are available:

-  .. co:: COMPRESS
      :choices: NONE, DEFLATE, JPEG, JPEG2000
      :default: DEFLATE

      Set the compression to use for raster data.

-  .. co:: STREAM_COMPRESS
      :choices: NONE, DEFLATE
      :default: DEFLATE

      Set the compression to use for
      stream objects (vector geometries, JavaScript content).

-  .. co:: DPI
      :default: 72

      Set the DPI to use. May be
      automatically adjusted to higher value so that page dimension does
      not exceed the 14400 maximum value (in user units) allowed by
      Acrobat.

-  .. co:: WRITE_USERUNIT
      :choices: YES, NO
      :since: 2.2

      Whether the UserUnit setting
      computed from the DPI (UserUnit = DPI / 72.0) should be recorded in
      the file. When UserUnit is recorded, the raster size in pixels
      recognized by GDAL on reading remains identical to the source raster.
      When UserUnit is not recorded, the printed size will depends on the
      DPI value. If this parameter is not set, but DPI is specified, then
      it will default to NO (so that the printed size depends on the DPI
      value). If this parameter is not set and DPI is not specified, then
      UserUnit will be recorded (so that the raster size in pixels
      recognized by GDAL on reading remain identical to the source raster).

-  .. co:: PREDICTOR
      :choices: 1, 2
      :default: 1

      Only for DEFLATE compression. Might be set to 2
      to use horizontal predictor that can make files smaller (but not
      always!).

-  .. co:: JPEG_QUALITY
      :choices: 1-100
      :default: 75

      Set the JPEG quality when using JPEG
      compression. A value of 100 is best quality (least compression), and
      1 is worst quality (best compression).

-  .. co:: JPEG2000_DRIVER
      :choices: JP2KAK, JP2ECW, JP2OpenJPEG, JPEG2000

      Set the
      JPEG2000 driver to use. If not specified, it will be searched in the
      previous list.

-  .. co:: TILED
      :choices: YES, NO
      :default: NO

      By default monoblock files are created. This option
      can be used to force creation of tiled PDF files.

-  .. co:: BLOCKXSIZE
      :default: 256

      Sets tile width.

-  .. co:: BLOCKYSIZE
      :default: 256

      Set tile height.

-  .. co:: CLIPPING_EXTENT
      :choices: xmin\,ymin\,xmax\,ymax

      Set the clipping extent for
      the main source dataset and for the optional extra rasters. The
      coordinates are expressed in the units of the SRS of the dataset. If
      not specified, the clipping extent is set to the extent of the main
      source dataset.

-  .. co:: LAYER_NAME

      Name for layer where the raster is placed. If
      specified, the raster will be be placed into a layer that can be
      toggled/un-toggled in the "Layer tree" of the PDF reader.

-  .. co:: EXTRA_RASTERS
      :choices: <dataset_ids>

      Comma separated list of georeferenced
      rasters to insert into the page. Those rasters are displayed on top
      of the main source raster. They must be georeferenced in the same
      projection, and they will be clipped to CLIPPING_EXTENT if it is
      specified (otherwise to the extent of the main source raster).

-  .. co:: EXTRA_RASTERS_LAYER_NAME
      :choices: <dataset_names>

      Comma separated list of
      name for each raster specified in EXTRA_RASTERS. If specified, each
      extra raster will be be placed into a layer, named with the specified
      value, that can be toggled/un-toggled in the "Layer tree" of the PDF
      reader. If not specified, all the extra rasters will be placed in the
      default layer.

-  .. co:: EXTRA_STREAM
      :choices: <content>

      A PDF content stream to draw after the
      imagery, typically to add some text. It may refer to any of the 14
      standard PDF Type 1 fonts (omitting hyphens), as /FTimesRoman,
      /FTimesBold, /FHelvetica, /FCourierOblique, ... , in which case the
      required resource dictionary will be inserted.

-  .. co:: EXTRA_IMAGES
      :choices: <image_file_name\,x\,y\,scale[\,link=some_url]> (possibly repeated)

      A list of (ungeoreferenced) images to insert into the
      page as extra content. This is useful to insert logos, legends,
      etc... x and y are in user units from the lower left corner of the
      page, and the anchor point is the lower left pixel of the image.
      scale is a magnifying ratio (use 1 if unsure). If link=some_url is
      specified, the image will be selectable and its selection will cause
      a web browser to be opened on the specified URL.

-  .. co:: EXTRA_LAYER_NAME

      Name for layer where the extra content
      specified with EXTRA_STREAM or EXTRA_IMAGES is placed. If specified,
      the extra content will be be placed into a layer that can be
      toggled/un-toggled in the "Layer tree" of the PDF reader.

-  .. co:: MARGIN

      Margin around image in user units.

-  .. co:: LEFT_MARGIN

      Margin left of image in user units.

-  .. co:: RIGHT_MARGIN

      Margin right of image in user units.

-  .. co:: TOP_MARGIN

      Margin above image in user units.

-  .. co:: BOTTOM_MARGIN

      Margin below image in user units.

-  .. co:: GEO_ENCODING
      :choices: NONE, ISO32000
      :default: ISO32000

      Set the Geo encoding method to use.

      Support for deprecated OGC_BP and BOTH options has been removed in GDAL 3.11

-  .. co:: NEATLINE
      :choices: <polygon_definition_in_wkt>

      Set the NEATLINE to use.

-  .. co:: XMP
      :choices: NONE, <xml_xmp_content>

      By default, if the source dataset has
      data in the 'xml:XMP' metadata domain, this data will be copied to
      the output PDF, unless this option is set to NONE. The XMP xml string
      can also be directly set to this option.

-  .. co:: WRITE_INFO
      :choices: YES, NO
      :default: YES

      By default, the AUTHOR, CREATOR,
      CREATION_DATE, KEYWORDS, PRODUCER, SUBJECT and TITLE information will
      be written into the PDF Info block from the corresponding metadata
      item from the source dataset, or if not set, from the corresponding
      creation option. If this option is set to NO, no information will be
      written.

-  .. co:: AUTHOR

      Author metadata to write into the PDF Info block.

-  .. co:: CREATOR

      Creator metadata to write into the PDF Info block.

-  .. co:: CREATION_DATE

      Create date metadata to write into the PDF Info block. The format of
      the value must be D:YYYYMMDDHHmmSSOHH'mm' (e.g.
      D:20121122132447+02'00' for 22 nov 2012 13:24:47 GMT+02) (see `PDF
      Reference, version
      1.7 <http://www.adobe.com/devnet/acrobat/pdfs/pdf_reference_1-7.pdf>`__,
      page 160)

-  .. co:: KEYWORDS

      Keywords metadata to write into the PDF Info block.

-  .. co:: PRODUCER

      Producer metadata to write into the PDF Info block.

-  .. co:: SUBJECT

      Subject metadata to write into the PDF Info block.

-  .. co:: TITLE

      Title metadata to write into the PDF Info block.

-  .. co:: OGR_DATASOURCE

      Name of the OGR datasource to display on
      top of the raster layer.

-  .. co:: OGR_DISPLAY_FIELD

      Name of the field (matching the name of
      a field from the OGR layer definition) to use to build the label of
      features that appear in the "Model Tree" UI component of a well-known
      PDF viewer. For example, if the OGR layer has a field called "ID",
      this can be used as the value for that option : features in the
      "Model Tree" will be labelled from their value for the "ID" field. If
      not specified, sequential generic labels will be used ("feature1",
      "feature2", etc... ).

-  .. co:: OGR_DISPLAY_LAYER_NAMES

      Comma separated list of names to
      display for the OGR layers in the "Model Tree". This option is useful
      to provide custom names, instead of OGR layer name that are used when
      this option is not specified. When specified, the number of names
      should be the same as the number of OGR layers in the datasource (and
      in the order they appear when listed by ogrinfo for example).

-  .. co:: OGR_WRITE_ATTRIBUTES
      :choices: YES, NO
      :default: YES

      Whether to write attributes of OGR features.

-  .. co:: OGR_LINK_FIELD

      Name of the field (matching the name of a
      field from the OGR layer definition) to use to cause clicks on OGR
      features to open a web browser on the URL specified by the field
      value.

-  .. co:: OFF_LAYERS

      Comma separated list of layer names that should
      be initially hidden. By default, all layers are visible. The layer
      names can come from LAYER_NAME (main raster layer name),
      EXTRA_RASTERS_LAYER_NAME, EXTRA_LAYER_NAME and
      OGR_DISPLAY_LAYER_NAMES.

-  .. co:: EXCLUSIVE_LAYERS

      Comma separated list of layer names, such
      that only one of those layers can be visible at a time. This is the
      behavior of radio-buttons in a graphical user interface. The layer
      names can come from LAYER_NAME (main raster layer name),
      EXTRA_RASTERS_LAYER_NAME, EXTRA_LAYER_NAME and
      OGR_DISPLAY_LAYER_NAMES.

-  .. co:: JAVASCRIPT
      :choices: <script>

      Javascript content to run at document opening.
      See `Acrobat(R) JavaScript Scripting
      Reference <http://partners.adobe.com/public/developer/en/acrobat/sdk/AcroJS.pdf>`__.

-  .. co:: JAVASCRIPT_FILE
      :choices: <filename>

      Name of Javascript file to embed
      and run at document opening. See `Acrobat(R) JavaScript Scripting
      Reference <http://partners.adobe.com/public/developer/en/acrobat/sdk/AcroJS.pdf>`__.

-  .. co:: COMPOSITION_FILE
      :choices: <xml_filename>
      :since: 3.0

      See below
      paragraph "Creation of PDF file from a XML composition file"

Update of existing files
------------------------

Existing PDF files (created or not with GDAL) can be opened in update
mode in order to set or update the following elements :

-  Geotransform and associated projection (with SetGeoTransform() and
   SetProjection())
-  GCPs (with SetGCPs())
-  Neatline (with SetMetadataItem("NEATLINE",
   polygon_definition_in_wkt))
-  Content of Info object (with SetMetadataItem(key, value) where key is
   one of AUTHOR, CREATOR, CREATION_DATE, KEYWORDS, PRODUCER, SUBJECT
   and TITLE)
-  xml:XMP metadata (with SetMetadata(md, "xml:XMP"))

For geotransform or GCPs, the Geo encoding method used by default is
ISO32000.

Updated elements are written at the end of the file, following the
incremental update method described in the PDF specification.

Creation of PDF file from a XML composition file (GDAL >= 3.0)
--------------------------------------------------------------

A PDF file can be generate from a XML file that describes the
composition of the PDF:

-  number of pages
-  layer tree, with visibility state, exclusion groups
-  definition or 0, 1 or several georeferenced areas per page
-  page content made of rasters, vectors or labels

The GDALCreate() API must be used with width = height = bands = 0 and
datatype = GDT_Unknown and :co:`COMPOSITION_FILE` must be the single creation
option.

The XML schema against which the composition file must validate is
`pdfcomposition.xsd <https://raw.githubusercontent.com/OSGeo/gdal/master/frmts/pdf/data/pdfcomposition.xsd>`__

Example on how to use the API:

.. code-block:: c++

   char** papszOptions = CSLSetNameValue(nullptr, "COMPOSITION_FILE", "the.xml");
   GDALDataset* ds = GDALCreate("the.pdf", 0, 0, 0, GDT_Unknown, papszOptions);
   // return a non-null (fake) dataset in case of success, nullptr otherwise.
   GDALClose(ds);
   CSLDestroy(papszOptions);

A sample Python script
`gdal_create_pdf.py <https://raw.githubusercontent.com/OSGeo/gdal/master/swig/python/gdal-utils/osgeo_utils/samples/gdal_create_pdf.py>`__
is also available. Starting with GDAL 3.2, the :ref:`gdal_create` utility can
also be used.

Example of a composition XML file:

.. code-block:: xml

   <PDFComposition>
       <Metadata>
           <Author>Even</Author>
       </Metadata>

       <LayerTree displayOnlyOnVisiblePages="true">
           <Layer id="l1" name="Satellite imagery"/>
           <Layer id="l2" name="OSM data">
               <Layer id="l2.1" name="Roads" initiallyVisible="false"/>
               <Layer id="l2.2" name="Buildings" mutuallyExclusiveGroupId="group1">
                   <Layer id="l2.2.text" name="Buildings name"/>
               </Layer>
               <Layer id="l2.3" name="Cadastral parcels" mutuallyExclusiveGroupId="group1"/>
           </Layer>
       </LayerTree>

       <Page id="page_1">
           <DPI>72</DPI>
           <Width>10</Width>
           <Height>15</Height>
           <Georeferencing id="georeferenced">
               <SRS dataAxisToSRSAxisMapping="2,1">EPSG:4326</SRS>
               <BoundingBox x1="1" y1="1" x2="9" y2="14"/>
               <BoundingPolygon>POLYGON((1 1,9 1,9 14,1 14,1 1))</BoundingPolygon>
               <ControlPoint x="1"  y="1"  GeoY="48"  GeoX="2"/>
               <ControlPoint x="1"  y="14" GeoY="49"  GeoX="2"/>
               <ControlPoint x="9"  y="1"  GeoY="49"  GeoX="3"/>
               <ControlPoint x="9"  y="14" GeoY="48"  GeoX="3"/>
           </Georeferencing>

           <Content>
               <IfLayerOn layerId="l1">
                   <!-- image drawn, and stretched to (x1,y1)->(x2,y2), without reading its georeferencing -->
                   <Raster dataset="satellite.png" x1="1" y1="1" x2="9" y2="14"/>
               </IfLayerOn>
               <IfLayerOn layerId="l2">
                   <IfLayerOn layerId="l2.1">
                       <Raster dataset="roads.jpg" x1="1" y1="1" x2="9" y2="14"/>
                       <!-- vector drawn with coordinates in PDF coordinate space -->
                       <Vector dataset="roads_pdf_units.shp" layer="roads_pdf_units" visible="false">
                           <LogicalStructure displayLayerName="Roads" fieldToDisplay="road_name"/>>
                       </Vector>
                   </IfLayerOn>
                   <IfLayerOn layerId="l2.2">
                       <!-- image drawn by taking into account its georeferencing -->
                       <Raster dataset="buildings.tif" georeferencingId="georeferenced"/>
                       <IfLayerOn layerId="l2.2.text">
                           <!-- vector drawn by taking into account its georeferenced coordinates -->
                           <VectorLabel dataset="labels.shp" layer="labels" georeferencingId="georeferenced">
                           </VectorLabel>
                       </IfLayerOn>
                   </IfLayerOn>
                   <IfLayerOn layerId="l2.3">
                       <PDF dataset="parcels.pdf">
                           <Blending function="Normal" opacity="0.7"/>
                       </PDF>
                   </IfLayerOn>
               </IfLayerOn>
           </Content>
       </Page>

       <Page id="page_2">
           <DPI>72</DPI>
           <Width>10</Width>
           <Height>15</Height>
           <Content>
           </Content>
       </Page>

       <Outline>
           <OutlineItem name="turn only layer 'Satellite imagery' on, and switch to fullscreen" italic="true" bold="true">
               <Actions>
                   <SetAllLayersStateAction visible="false"/>
                   <SetLayerStateAction visible="true" layerId="l1"/>
                   <JavascriptAction>app.fs.isFullScreen = true;</JavascriptAction>
               </Actions>
           </OutlineItem>
           <OutlineItem name="Page 1" pageId="page_1">
               <OutlineItem name="Important feature !">
                   <Actions>
                       <GotoPageAction pageId="page_1" x1="1" y1="2" x2="3" y2="4"/>
                   </Actions>
               </OutlineItem>
           </OutlineItem>
           <OutlineItem name="Page 2" pageId="page_2"/>
       </Outline>

   </PDFComposition>

Build dependencies
------------------

For read support, GDAL must be built against one of the following
libraries :

-  `Poppler <http://poppler.freedesktop.org/>`__ (GPL-licensed)
-  `PoDoFo <http://podofo.sourceforge.net/>`__ (LGPL-licensed)
-  `PDFium <https://code.google.com/p/pdfium/>`__ (New BSD-licensed,
   supported since GDAL 2.1.0)

Note: it is also possible to build against a combination of several of
the above libraries. PDFium will be used in priority over Poppler,
itself used in priority over PoDoFo.

Unix build
~~~~~~~~~~

The relevant configure options are --with-poppler, --with-podofo,
--with-podofo-lib and --with-podofo-extra-lib-for-test.

Starting with GDAL 2.1.0, --with-pdfium, --with-pdfium-lib,
--with-pdfium-extra-lib-for-test and --enable-pdf-plugin are also
available.

Poppler
~~~~~~~

libpoppler itself must have been configured with
-DENABLE_UNSTABLE_API_ABI_HEADERS=ON
so that the xpdf C++ headers are available. Note: the poppler C++ API
isn't stable, so the driver compilation may fail with too old or too
recent poppler versions.

PoDoFo
~~~~~~

As a partial alternative, the PDF driver can be compiled against
libpodofo to avoid the libpoppler dependency. This is sufficient to get
the georeferencing and vector information. However, for getting the
imagery, the pdftoppm utility that comes with the poppler distribution
must be available in the system PATH. A temporary file will be generated
in a directory determined by the following configuration options :
:config:`CPL_TMPDIR`, ``TMPDIR`` or ``TEMP`` (in that order). If none are defined, the
current directory will be used. Successfully tested versions are
libpodofo 0.8.4, 0.9.1 and 0.9.3. Important note: using PoDoFo 0.9.0 is
strongly discouraged, as it could cause crashes in GDAL due to a bug in
PoDoFo.

PDFium
~~~~~~

Using PDFium as a backend allows access to raster, vector,
georeferencing and other metadata. The PDFium backend has also support
for arbitrary overviews, for fast zoom-out.

Only GDAL builds against static builds of PDFium have been tested.
Building PDFium can be challenging, and particular builds must be used to
work properly with GDAL.

With GDAL >= 3.11
+++++++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_11>`__
repository must be used to build a patched version of PDFium.

With GDAL = 3.10
++++++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_10>`__
repository must be used to build a patched version of PDFium.

With GDAL = 3.9
+++++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_9>`__
repository must be used to build a patched version of PDFium.

With GDAL = 3.8
+++++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_8>`__
repository must be used to build a patched version of PDFium.

With GDAL 3.7
+++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_7>`__
repository must be used to build a patched version of PDFium.

With GDAL 3.5 and 3.6
+++++++++++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_5>`__
repository must be used to build a patched version of PDFium.

With GDAL 3.4
+++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_4>`__
repository must be used to build a patched version of PDFium.

With GDAL 3.2 and 3.3
+++++++++++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_2>`__
repository must be used to build a patched version of PDFium.

With GDAL 3.1.x
+++++++++++++++

The scripts in the `<https://github.com/rouault/pdfium_build_gdal_3_1>`__
repository must be used to build a patched version of PDFium.

Examples
--------

-  Create a PDF from 2 rasters (main_raster and another_raster), such
   that main_raster is initially displayed, and they are exclusively
   displayed :

   ::

      gdal_translate -of PDF main_raster.tif my.pdf -co LAYER_NAME=main_raster
                     -co EXTRA_RASTERS=another_raster.tif -co EXTRA_RASTERS_LAYER_NAME=another_raster
                     -co OFF_LAYERS=another_raster -co EXCLUSIVE_LAYERS=main_raster,another_raster

-  Create of PDF with some JavaScript :

   ::

      gdal_translate -of PDF my.tif my.pdf -co JAVASCRIPT_FILE=script.js

   where script.js is :

   ::

      button = app.alert({cMsg: 'This file was generated by GDAL. Do you want to visit its website ?', cTitle: 'Question', nIcon:2, nType:2});
      if (button == 4) app.launchURL('http://gdal.org/');

See also
--------

:ref:`PDF vector <vector.pdf>` documentation page

Specifications :

-  `Adobe Supplement to ISO
   32000 <http://www.adobe.com/devnet/acrobat/pdfs/adobe_supplement_iso32000.pdf>`__
-  `PDF Reference, version
   1.7 <http://www.adobe.com/devnet/acrobat/pdfs/pdf_reference_1-7.pdf>`__
-  `Acrobat(R) JavaScript Scripting
   Reference <http://partners.adobe.com/public/developer/en/acrobat/sdk/AcroJS.pdf>`__
-  `OGC PDF Georegistration Encoding Best Practice Version 2.2
   (08-139r3) <http://portal.opengeospatial.org/files/?artifact_id=40537>`__ (not recommended)

Libraries :

-  `Poppler homepage <http://poppler.freedesktop.org/>`__
-  `PoDoFo homepage <http://podofo.sourceforge.net/>`__
-  `PDFium homepage <https://code.google.com/p/pdfium/>`__
-  `PDFium forked version for simpler
   builds <https://github.com/rouault/pdfium>`__

Samples :

-  `A few Geospatial PDF
   samples <https://www.terragotech.com/learn-more/sample-geopdfs>`__
-  `Tutorial to generate Geospatial PDF maps from OSM
   data <http://latuviitta.org/documents/Geospatial_PDF_maps_from_OSM_with_GDAL.pdf>`__
