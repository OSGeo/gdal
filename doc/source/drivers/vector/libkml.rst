.. _vector.libkml:

LIBKML Driver (.kml .kmz)
=========================

.. shortname:: LIBKML

.. build_dependencies:: libkml

The LIBKML driver is a client of
`Libkml <https://github.com/libkml/libkml>`__ , a reference
implementation of `KML <http://www.opengeospatial.org/standards/kml/>`__
reading and writing, in the form of a cross platform C++ library. You
must build and install Libkml in order to use this OGR driver. Note: you
need to build libkml 1.3 or master.

Note that if you build and include this LIBKML driver, it will become
the default reader of KML for ogr, overriding the previous :ref:`KML
driver <vector.kml>`. You can still specify either KML or LIBKML as
the output driver via the command line

Libkml from Google provides reading services for any valid KML file.
However, please be advised that some KML facilities do not map into the
Simple Features specification OGR uses as its internal structure.
Therefore, a best effort will be made by the driver to understand the
content of a KML file read by libkml into ogr, but your mileage may
vary. Please try a few KML files as samples to get a sense of what is
understood. In particular, nesting of feature sets more than one deep
will be flattened to support ogr's internal format.

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Datasource
----------

You may specify a datasource
as a kml file ``somefile.kml`` , a directory ``somedir/`` , or a kmz
file ``somefile.kmz`` .

By default on directory and kmz datasources, an index file of all the
layers will be read from or written to doc.kml. It contains a
`<NetworkLink> <https://developers.google.com/kml/documentation/kmlreference#networklink>`__
to each layer file in the datasource. This feature can be turned off by
setting the configuration option :config:`LIBKML_USE_DOC.KML`
to "NO":

-  .. config:: LIBKML_USE_DOC.KML
      :choices: YES, NO
      :default: YES

      Use a ``doc.kml`` index file.


StyleTable
~~~~~~~~~~

Datasource style tables are written to the
`<Document> <https://developers.google.com/kml/documentation/kmlreference#document>`__
in a .kml, style/style.kml in a kmz file, or style.kml in a directory,
as one or more
`<Style> <https://developers.google.com/kml/documentation/kmlreference#style>`__
elements. Not all of :ref:`ogr_feature_style` can translate into
KML.

Dataset creation options
~~~~~~~~~~~~~~~~~~~~~~~~

|about-dataset-creation-options|
The following dataset creation options can be
used to generate a
`<atom:Author> <https://developers.google.com/kml/documentation/kmlreference#atomauthor>`__
element at the top Document level.

- .. dsco:: AUTHOR_NAME

     Specifieds the value of the ``<atom:name>`` element.

- .. dsco:: AUTHOR_URI

     Specifieds the value of the ``<atom:uri>`` element.
     It should start with ``http://`` or ``https://``

- .. dsco:: AUTHOR_EMAIL

     Specifieds the value of the ``<atom:email>`` element.
     It should include a ``@`` character.

Additional dataset creation options affecting the top Document level:

- .. dsco:: LINK

     Specifies the href of an
     `<atom:link> <https://developers.google.com/kml/documentation/kmlreference#atomlink>`__
     element at the top Document level.

- .. dsco:: PHONENUMBER

     Specifies the value of the
     `<phoneNumber> <https://developers.google.com/kml/documentation/kmlreference#phonenumber>`__
     element at the top Document level. The value must follow the syntax of
     `IETF RFC 3966 <http://tools.ietf.org/html/rfc3966>`__.

- .. dsco:: DOCUMENT_ID
     :default: root_doc
     :since: 2.2

     Specifies the id of the root <Document> node.

Container properties
^^^^^^^^^^^^^^^^^^^^

The following dataset creation options can be used to set container
options :

-  .. dsco:: NAME

      `<name> <https://developers.google.com/kml/documentation/kmlreference#name>`__
      element

-  .. dsco:: VISIBILITY

      `<visibility> <https://developers.google.com/kml/documentation/kmlreference#visibility>`__
      element

-  .. dsco:: OPEN

      `<open> <https://developers.google.com/kml/documentation/kmlreference#open>`__
      element

-  .. dsco:: SNIPPET

      `<snippet> <https://developers.google.com/kml/documentation/kmlreference#snippet>`__
      element

-  .. dsco:: DESCRIPTION

      `<description> <https://developers.google.com/kml/documentation/kmlreference#description>`__
      element

List style
^^^^^^^^^^

The following dataset creation options can be used to control how the
main folder (folder of layers) appear in the Places panel of the Earth
browser, trough a
`<ListStyle> <https://developers.google.com/kml/documentation/kmlreference#liststyle>`__
element:

-  .. dsco:: LISTSTYLE_TYPE
      :choices: check, radioFolder, checkOffOnly, checkHideChildren

      Sets the
      `<listItemType> <https://developers.google.com/kml/documentation/kmlreference#listItemType>`__
      element.

-  .. dsco:: LISTSTYLE_ICON_HREF

      URL of the icon to display for the main
      folder. Sets the href element of the
      `<ItemIcon> <https://developers.google.com/kml/documentation/kmlreference#itemicon>`__
      element.

Balloon style
^^^^^^^^^^^^^

If a style *foo* is defined, it is possible to add a
`<BalloonStyle> <https://developers.google.com/kml/documentation/kmlreference#balloonstyle>`__
element to it, by specifying the **foo_BALLOONSTYLE_BGCOLOR** and/or
**foo_BALLOONSTYLE_TEXT** elements.

NetworkLinkControl
^^^^^^^^^^^^^^^^^^

A
`<NetworkLinkControl> <https://developers.google.com/kml/documentation/kmlreference#networklinkcontrol>`__
element can be defined if at least one of the following dataset creation
option is specified:

-  .. dsco:: NLC_MINREFRESHPERIOD

      to set the
      `<minRefreshPeriod> <https://developers.google.com/kml/documentation/kmlreference#minrefreshperiod>`__
      element

-  .. dsco:: NLC_MAXSESSIONLENGTH

      to set the
      `<maxSessionLength> <https://developers.google.com/kml/documentation/kmlreference#maxsessionlength>`__
      element

-  .. dsco:: NLC_COOKIE

      to set the
      `<cookie> <https://developers.google.com/kml/documentation/kmlreference#cookie>`__
      element

-  .. dsco:: NLC_MESSAGE

      to set the
      `<message> <https://developers.google.com/kml/documentation/kmlreference#message>`__
      element

-  .. dsco:: NLC_LINKNAME

      to set the
      `<linkName> <https://developers.google.com/kml/documentation/kmlreference#linkname>`__
      element

-  .. dsco:: NLC_LINKDESCRIPTION

      to set the
      `<linkDescription> <https://developers.google.com/kml/documentation/kmlreference#linkdescription>`__
      element

-  .. dsco:: NLC_LINKSNIPPET

      to set the
      `<linkSnippet> <https://developers.google.com/kml/documentation/kmlreference#linksnippet>`__
      element

-  .. dsco:: NLC_EXPIRES

      to set the
      `<expires> <https://developers.google.com/kml/documentation/kmlreference#expires>`__
      element

Update documents
^^^^^^^^^^^^^^^^

When defining the dataset creation option **UPDATE_TARGETHREF**, a
NetworkLinkControl KML file with an
`<Update> <https://developers.google.com/kml/documentation/kmlreference#update>`__
element will be generated. See the `tutorial about
update <https://developers.google.com/kml/documentation/updates>`__.

The CreateFeature() operation on a layer will be translated as a
`<Create> <https://developers.google.com/kml/documentation/kmlreference#create>`__
element.

The SetFeature() operation on a layer will be translated as a
`<Change> <https://developers.google.com/kml/documentation/kmlreference#change>`__
element.

The DeleteFeature() operation on a layer will be translated as a
`<Delete> <https://developers.google.com/kml/documentation/kmlreference#delete>`__
element.

Layer
-----

:cpp:class:`OGRLayer` are mapped
to kml files as a
`<Document> <https://developers.google.com/kml/documentation/kmlreference#document>`__
or
`<Folder> <https://developers.google.com/kml/documentation/kmlreference#folder>`__,
and in kmz files or directories as a separate kml file.

Style
~~~~~

Layer style tables can not be read from or written to a kml layer that
is a
`<Folder> <https://developers.google.com/kml/documentation/kmlreference#folder>`__,
otherwise they are in the
`<Document> <https://developers.google.com/kml/documentation/kmlreference#document>`__
that is the layer.

Schema
~~~~~~

Read and write of
`<Schema> <https://developers.google.com/kml/documentation/kmlreference#schema>`__
is supported for .kml files, .kmz files, and directories.

Layer creation options
~~~~~~~~~~~~~~~~~~~~~~

|about-layer-creation-options|
The following layer creation options can be used
to generate a
`<LookAt> <https://developers.google.com/kml/documentation/kmlreference#lookat>`__
element at the layer level.

-  .. lco:: LOOKAT_LONGITUDE
      :required: YES

-  .. lco:: LOOKAT_LATITUDE
      :required: YES

-  .. lco:: LOOKAT_RANGE
      :required: YES

-  .. lco:: LOOKAT_HEADING
-  .. lco:: LOOKAT_TILT
-  .. lco:: LOOKAT_ALTITUDE
-  .. lco:: LOOKAT_ALTITUDEMODE

Alternatively, a
`<Camera> <https://developers.google.com/kml/documentation/kmlreference#camera>`__
element can be generated.

-  .. lco:: CAMERA_LONGITUDE
      :required: YES

-  .. lco:: CAMERA_LATITUDE
      :required: YES

-  .. lco:: CAMERA_ALTITUDE
      :required: YES

-  .. lco:: CAMERA_ALTITUDEMODE
      :required: YES

-  .. lco:: CAMERA_HEADING
-  .. lco:: CAMERA_TILT
-  .. lco:: CAMERA_ROLL

A
`<Region> <https://developers.google.com/kml/documentation/kmlreference#region>`__
element can be generated to control when objects of the layer are
visible or not. If :lco:`REGION_XMIN`, :lco:`REGION_YMIN`, :lco:`REGION_XMAX` and
:lco:`REGION_YMAX`, the region coordinates are determined from the spatial
extent of the features being written in the layer.

-  .. lco:: ADD_REGION
      :choices: YES, NO
      :default: NO

-  .. lco:: REGION_XMIN

      defines the west coordinate of the region.

-  .. lco:: REGION_YMIN

      defines the south coordinate of the region.

-  .. lco:: REGION_XMAX

      defines the east coordinate of the region.

-  .. lco:: REGION_YMAX

      defines the north coordinate of the region.

-  .. lco:: REGION_MIN_LOD_PIXELS
      :default: 256

      minimum size in pixels of the region so that it is displayed.

-  .. lco:: REGION_MAX_LOD_PIXELS
      :default: -1

      maximum size in pixels of the
      region so that it is displayed. Defaults to -1 (infinite).

-  .. lco:: REGION_MIN_FADE_EXTENT
      :default: 0

      distance over which the
      geometry fades, from fully opaque to fully transparent.

-  .. lco:: REGION_MAX_FADE_EXTENT
      :default: 0

      distance over which the
      geometry fades, from fully transparent to fully opaque.


A
`<ScreenOverlay> <https://developers.google.com/kml/documentation/kmlreference#screenoverlay>`__
element can be added to display a logo, a legend, etc...

-  .. lco:: SO_HREF
      :required: YES

      URL of the image to display.

-  .. lco:: SO_NAME
-  .. lco:: SO_DESCRIPTION
-  .. lco:: SO_OVERLAY_X
-  .. lco:: SO_OVERLAY_Y
-  .. lco:: SO_OVERLAY_XUNITS
-  .. lco:: SO_OVERLAY_YUNITS
-  .. lco:: SO_SCREEN_X
      :default: 0.05
-  .. lco:: SO_SCREEN_Y
      :default: 0.05
-  .. lco:: SO_SCREEN_XUNITS
      :default: Fraction
-  .. lco:: SO_SCREEN_YUNITS
      :default: Fraction
-  .. lco:: SO_SIZE_X
-  .. lco:: SO_SIZE_Y
-  .. lco:: SO_SIZE_XUNITS
-  .. lco:: SO_SIZE_YUNITS


The following option controls whether layers are written as a Document or a Folder:

-  .. lco:: FOLDER
      :choices: YES, NO

      By default, layers are written as
      `<Document> <https://developers.google.com/kml/documentation/kmlreference#document>`__
      elements. By setting this option to YES, it is
      also possible to write them as
      `<Folder> <https://developers.google.com/kml/documentation/kmlreference#folder>`__
      elements (only in .kml files).

The following layer creation options can be used to set container
options :

-  .. lco:: NAME

      `<name> <https://developers.google.com/kml/documentation/kmlreference#name>`__
      element

-  .. lco:: VISIBILITY

      `<visibility> <https://developers.google.com/kml/documentation/kmlreference#visibility>`__
      element

-  .. lco:: OPEN

      `<open> <https://developers.google.com/kml/documentation/kmlreference#open>`__
      element

-  .. lco:: SNIPPET

      `<snippet> <https://developers.google.com/kml/documentation/kmlreference#snippet>`__
      element

-  .. lco:: DESCRIPTION

      `<description> <https://developers.google.com/kml/documentation/kmlreference#description>`__
      element

Do not confuse them with the same named dataset creation options.

The following layer creation options can be used to control how the
folder of a layer appear in the Places panel of the Earth browser,
trough a
`<ListStyle> <https://developers.google.com/kml/documentation/kmlreference#liststyle>`__
element:

-  .. lco:: LISTSTYLE_TYPE
      :choices: check, radioFolder, checkOffOnly, checkHideChildren

      Sets the
      `<listItemType> <https://developers.google.com/kml/documentation/kmlreference#listItemType>`__
      element.

-  .. lco:: LISTSTYLE_ICON_HREF

      URL of the icon to display for the layer
      folder. Sets the href element of the
      `<ItemIcon> <https://developers.google.com/kml/documentation/kmlreference#itemicon>`__
      element.

Feature
-------

An :cpp:class:`OGRFeature`
generally translates to kml as a
`<Placemark> <https://developers.google.com/kml/documentation/kmlreference#placemark>`__,
and vice-versa.

If the model field is defined, a
`<Model> <https://developers.google.com/kml/documentation/kmlreference#model>`__
object within the Placemark will be generated.

If the networklink field is defined, a
`<NetworkLink> <https://developers.google.com/kml/documentation/kmlreference#networklink>`__
will be generated. Other networklink fields are optional.

If the photooverlay field is defined, a
`<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__
will be generated (provided that the camera_longitude, camera_latitude,
camera_altitude, camera_altitudemode, head and/or tilt and/or roll,
leftfov, rightfov, bottomfov, topfov, near fields are also set. The
shape field is optional.

In case the PhotoOverlay is a big image, it is highly recommended to
tile it and generate overview levels, as explained in the `PhotoOverlay
tutorial <https://developers.google.com/kml/documentation/photos>`__. In
which case, the URL should contain the "$[level]", "$[x]" and "$[y]"
sub-strings in the photooverlay field, and the imagepyramid_tilesize,
imagepyramid_maxwidth, imagepyramid_maxheight and
imagepyramid_gridorigin fields should be set.

Placemark, Model, NetworkLink and PhotoOverlay objects can have an
associated camera if the camera_longitude, camera_latitude,
camera_altitude, camera_altitudemode, head and/or tilt and/or roll
fields are defined.

KML `<GroundOverlay> <https://developers.google.com/kml/documentation/kmlreference#groundoverlay>`__
elements are supported for reading (unless the
:config:`LIBKML_READ_GROUND_OVERLAY` configuration option is set to FALSE). For
such elements, there are icon and drawOrder fields.

The following configuration options affect reading of features:

-  .. config:: LIBKML_READ_GROUND_OVERLAY
      :choices: TRUE, FALSE
      :default: TRUE

      If ``FALSE``, skip reading GroundOverlay elements.

.. _style-1:

Style
~~~~~

Style Strings at the feature level are Mapped to KML as either a
`<Style> <https://developers.google.com/kml/documentation/kmlreference#style>`__
or
`<StyleUrl> <https://developers.google.com/kml/documentation/kmlreference#styleurl>`__
in each
`<Placemark> <https://developers.google.com/kml/documentation/kmlreference#placemark>`__.

The following configuration options affect handling of styles:

-  .. config:: LIBKML_RESOLVE_STYLE
      :choices: YES, NO

      When reading a kml feature and this option
      is set to yes, styleurls are looked up in the style
      tables and the features style string is set to the style from the table.
      This is to allow reading of shared styles by applications, like
      MapServer, that do not read style tables.

-  .. config:: LIBKML_EXTERNAL_STYLE
      :choices: YES, NO

      When reading a kml feature and this option
      is set to yes, a styleurl that is external to the
      datasource is read from disk or fetched from the server and parsed into
      the datasource style table. If the style kml can not be read or
      :config:`LIBKML_EXTERNAL_STYLE=NO` then the styleurl is copied to the
      style string.


-  .. config:: LIBKML_STYLEMAP_KEY

      When reading a kml StyleMap the default mapping is set to normal. If you
      wish to use the highlighted styles set this configuration option
      to "highlight"

When writing a kml, if there exist 2 styles of the form
"astylename_normal" and "astylename_highlight" (where astylename is any
string), then a StyleMap object will be creating from both styles and
called "astylename".

Fields
------

OGR fields (feature attributes) are mapped to kml with
`<Schema> <https://developers.google.com/kml/documentation/kmlreference#schema>`__;
and
`<SimpleData> <https://developers.google.com/kml/documentation/kmlreference#simpledata>`__,
except for some special fields as noted below.

.. note::

   It is also possible to export fields as
   `<Data> <https://developers.google.com/kml/documentation/kmlreference#data>`__
   elements using the following configuration option:

  .. config:: LIBKML_USE_SIMPLEFIELD
     :choices: YES, NO

     If ``NO``, export fields as <Data>.

A rich set of :ref:`configuration options <configoptions>` are
available to define how fields in input and output, map to a KML
`<Placemark> <https://developers.google.com/kml/documentation/kmlreference#placemark>`__.
For example, if you want a field called 'Cities' to map to the
`<name> <https://developers.google.com/kml/documentation/kmlreference#name>`__;
tag in KML, you can set a configuration option. Note these are independent of layer
creation and dataset creation options' `<name>`.

-  .. config:: LIBKML_NAME_FIELD
      :default: name

      Name of the string field that maps to the kml tag
      `<name> <https://developers.google.com/kml/documentation/kmlreference#name>`__.

-  .. config:: LIBKML_DESCRIPTION_FIELD
      :default: description

      Name of the string field that maps to the kml tag
      `<description> <https://developers.google.com/kml/documentation/kmlreference#description>`__.

-  .. config:: LIBKML_TIMESTAMP_FIELD
      :default: timestamp

      Name of the string/datetime/date/time field that maps to the kml tag
      `<timestamp> <https://developers.google.com/kml/documentation/kmlreference#timestamp>`__.

-  .. config:: LIBKML_BEGIN_FIELD
      :default: begin

      Name of the string/datetime/date/time field that maps to the kml tag
      `<begin> <https://developers.google.com/kml/documentation/kmlreference#begin>`__.

-  .. config:: LIBKML_END_FIELD
      :default: end

      Name of the string/datetime/date/time field that maps to the kml tag
      `<end> <https://developers.google.com/kml/documentation/kmlreference#end>`__.

-  .. config:: LIBKML_ALTITUDEMODE_FIELD
      :default: altitudeMode

      Name of the string field that maps to the kml tag
      `<altitudeMode> <https://developers.google.com/kml/documentation/kmlreference#altitudemode>`__
      or
      `<gx:altitudeMode> <https://developers.google.com/kml/documentation/kmlreference#gxaltitudemode>`__.

-  .. config:: LIBKML_TESSELLATE_FIELD
      :default: tessellate

      Name of the integer field that maps to the kml tag
      `<tessellate> <https://developers.google.com/kml/documentation/kmlreference#tessellate>`__.

-  .. config:: LIBKML_EXTRUDE_FIELD
      :default: extrude

      Name of the integer field that maps to the kml tag
      `<extrude> <https://developers.google.com/kml/documentation/kmlreference#extrude>`__.

-  .. config:: LIBKML_VISIBILITY_FIELD
      :default: visibility

      Name of the integer field that maps to the kml tag
      `<visibility> <https://developers.google.com/kml/documentation/kmlreference#visibility>`__.

-  .. config:: LIBKML_ICON_FIELD
      :default: icon

      Name of the string field that maps to the kml tag
      `<icon> <https://developers.google.com/kml/documentation/kmlreference#icon>`__.

-  .. config:: LIBKML_DRAWORDER_FIELD
      :default: drawOrder

      Name of the integer field that maps to the kml tag
      `<drawOrder> <https://developers.google.com/kml/documentation/kmlreference#draworder>`__.

-  .. config:: LIBKML_SNIPPET_FIELD
      :default: snippet

      Name of the integer field that maps to the kml tag
      `<snippet> <https://developers.google.com/kml/documentation/kmlreference#snippet>`__.

-  .. config:: LIBKML_HEADING_FIELD
      :default: heading

      Name of the real field that maps to the kml tag
      `<heading> <https://developers.google.com/kml/documentation/kmlreference#heading>`__.
      When reading, this field is present
      only if a Placemark has a Camera with a heading element.

-  .. config:: LIBKML_TILT_FIELD
      :default: tilt

      Name of the real field that maps to the kml tag
      `<tilt> <https://developers.google.com/kml/documentation/kmlreference#tilt>`__.
      When reading, this field is present only
      if a Placemark has a Camera with a tilt element.

-  .. config:: LIBKML_ROLL_FIELD
      :default: roll

      Name of the real field that maps to the kml tag
      `<roll> <https://developers.google.com/kml/documentation/kmlreference#roll>`__.
      When reading, this field is present only
      if a Placemark has a Camera with a roll element.

-  .. config:: LIBKML_MODEL_FIELD
      :default: model

      Name of the string field that can be used to define the URL of a 3D
      `<model> <https://developers.google.com/kml/documentation/kmlreference#model>`__.

-  .. config:: LIBKML_SCALE_X_FIELD
      :default: scale_x

      Name of the real field that maps to the x element of the kml tag
      `<scale> <https://developers.google.com/kml/documentation/kmlreference#scale>`__
      for a 3D model.

-  .. config:: LIBKML_SCALE_Y_FIELD
      :default: scale_y

      Name of the real field that maps to the y element of the kml tag
      `<scale> <https://developers.google.com/kml/documentation/kmlreference#scale>`__\ for
      a 3D model.

-  .. config:: LIBKML_SCALE_Z_FIELD
      :default: scale_z

      Name of the real field that maps to the z element of the kml tag
      `<scale> <https://developers.google.com/kml/documentation/kmlreference#scale>`__\ for
      a 3D model.

-  .. config:: LIBKML_NETWORKLINK_FIELD
      :default: networklink

      Name of the string field that maps to the href element of the kml tag
      `<href> <https://developers.google.com/kml/documentation/kmlreference#href>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_REFRESHVISIBILITY_FIELD
      :default: networklink_refreshvisibility

      Name of the integer field that maps to kml tag
      `<refreshVisibility> <https://developers.google.com/kml/documentation/kmlreference#refreshvisibility>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_FLYTOVIEW_FIELD
      :default: networklink_flytoview

      Name of the integer field that maps to kml tag
      `<flyToView> <https://developers.google.com/kml/documentation/kmlreference#flytoview>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_REFRESHMODE_FIELD
      :default: networklink_refreshmode

      Name of the string field that maps to kml tag
      `<refreshMode> <https://developers.google.com/kml/documentation/kmlreference#refreshmode>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_REFRESHINTERVAL_FIELD
      :default: networklink_refreshinterval

      Name of the real field that maps to kml tag
      `<refreshInterval> <https://developers.google.com/kml/documentation/kmlreference#refreshinterval>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_VIEWREFRESHMODE_FIELD
      :default: networklink_viewrefreshmode

      Name of the string field that maps to kml tag
      `<viewRefreshMode> <https://developers.google.com/kml/documentation/kmlreference#viewrefreshmode>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_VIEWREFRESHTIME_FIELD
      :default: networklink_viewrefreshtime

      Name of the real field that maps to kml tag
      `<viewRefreshTime> <https://developers.google.com/kml/documentation/kmlreference#viewrefreshtime>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_VIEWBOUNDSCALE_FIELD
      :default: networklink_viewboundscale

      Name of the real field that maps to kml tag
      `<viewBoundScale> <https://developers.google.com/kml/documentation/kmlreference#viewboundscale>`__
      of a NetworkLink.

-  .. config::LIBKML_NETWORKLINK_VIEWFORMAT_FIELD
      :default: networklink_viewformat

      Name of the string field that maps to kml tag
      `<viewFormat> <https://developers.google.com/kml/documentation/kmlreference#viewformat>`__
      of a NetworkLink.

-  .. config:: LIBKML_NETWORKLINK_HTTPQUERY_FIELD
      :default: networklink_httpquery

      Name of the string field that maps to kml tag
      `<httpQuery> <https://developers.google.com/kml/documentation/kmlreference#httpquery>`__
      of a NetworkLink.

-  .. config:: LIBKML_CAMERA_LONGITUDE_FIELD
      :default: camera_longitude

      Name of the real field that maps to kml tag
      `<longitude> <https://developers.google.com/kml/documentation/kmlreference#longitude>`__
      of a
      `<Camera> <https://developers.google.com/kml/documentation/kmlreference#camera>`__.

-  .. config::LIBKML_CAMERA_LATITUDE_FIELD
      :default: camera_latitude

      Name of the real field that maps to kml tag
      `<latitude> <https://developers.google.com/kml/documentation/kmlreference#latitude>`__
      of a
      `<Camera> <https://developers.google.com/kml/documentation/kmlreference#camera>`__.

-  .. config:: LIBKML_CAMERA_ALTITUDE_FIELD
      :default: camera_altitude

      Name of the real field that maps to kml tag
      `<altitude> <https://developers.google.com/kml/documentation/kmlreference#altitude>`__
      of a
      `<Camera> <https://developers.google.com/kml/documentation/kmlreference#camera>`__.

-  .. config:: LIBKML_CAMERA_ALTITUDEMODE_FIELD
      :default: camera_altitudemode

      Name of the real field that maps to kml tag
      `<altitudeMode> <https://developers.google.com/kml/documentation/kmlreference#altitudemode>`__
      of a
      `<Camera> <https://developers.google.com/kml/documentation/kmlreference#camera>`__.

-  .. config:: LIBKML_PHOTOOVERLAY_FIELD
      :default: photooverlay

      Name of the string field that maps to the href element of the kml tag
      `<href> <https://developers.google.com/kml/documentation/kmlreference#href>`__
      of a
      `<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__.

-  .. config:: LIBKML_LEFTFOV_FIELD
      :default: leftfov

      Name of the real field that maps to to kml tag
      `<LeftFov> <https://developers.google.com/kml/documentation/kmlreference#leftfov>`__
      of a
      `<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__.

-  .. config:: LIBKML_RIGHTFOV_FIELD
      :default: rightfov

      Name of the real field that maps to to kml tag
      `<RightFov> <https://developers.google.com/kml/documentation/kmlreference#rightfov>`__
      of a
      `<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__.

-  .. config:: LIBKML_BOTTOMFOV_FIELD
      :default: bottomfov

      Name of the real field that maps to to kml tag
      `<BottomFov> <https://developers.google.com/kml/documentation/kmlreference#bottomfov>`__
      of a
      `<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__.

-  .. config:: LIBKML_TOPFOV_FIELD
      :default: topfov

      Name of the real field that maps to to kml tag
      `<TopFov> <https://developers.google.com/kml/documentation/kmlreference#topfov>`__
      of a
      `<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__.

-  .. config:: LIBKML_NEARFOV_FIELD
      :default: near

      Name of the real field that maps to to kml tag
      `<Near> <https://developers.google.com/kml/documentation/kmlreference#leftfov>`__
      of a
      `<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__.

-  .. config:: LIBKML_PHOTOOVERLAY_SHAPE_FIELD
      :default: shape

      Name of the string field that maps to to kml tag
      `<shape> <https://developers.google.com/kml/documentation/kmlreference#shape>`__
      of a
      `<PhotoOverlay> <https://developers.google.com/kml/documentation/kmlreference#photooverlay>`__.

-  .. config:: LIBKML_IMAGEPYRAMID_TILESIZE
      :default: imagepyramid_tilesize

      Name of the integer field that maps to to kml tag
      `<tileSize> <https://developers.google.com/kml/documentation/kmlreference#tilesize>`__
      of a
      `<ImagePyramid> <https://developers.google.com/kml/documentation/kmlreference#imagepyramid>`__.

-  .. config:: LIBKML_IMAGEPYRAMID_MAXWIDTH
      :default: imagepyramid_maxwidth

      Name of the integer field that maps to to kml tag
      `<maxWidth> <https://developers.google.com/kml/documentation/kmlreference#maxwidth>`__
      of a
      `<ImagePyramid> <https://developers.google.com/kml/documentation/kmlreference#imagepyramid>`__.

-  .. config:: LIBKML_IMAGEPYRAMID_MAXHEIGHT
      :default: imagepyramid_maxheight

      Name of the integer field that maps to to kml tag
      `<maxHeight> <https://developers.google.com/kml/documentation/kmlreference#maxheight>`__
      of a
      `<ImagePyramid> <https://developers.google.com/kml/documentation/kmlreference#imagepyramid>`__.

-  .. config:: LIBKML_IMAGEPYRAMID_GRIDORIGIN
      :default: imagepyramid_gridorigin

      Name of the string field that maps to to kml tag
      `<gridOrigin> <https://developers.google.com/kml/documentation/kmlreference#maxheight>`__
      of a
      `<ImagePyramid> <https://developers.google.com/kml/documentation/kmlreference#imagepyramid>`__.

-  .. config:: OGR_STYLE

      string field that maps to a features style string, OGR reads this
      field if there is no style string set on the feature.

Geometry
--------

Translation of :cpp:class:`OGRGeometry` to
KML Geometry is pretty straightforward with only a couple of exceptions.
Point to
`<Point> <https://developers.google.com/kml/documentation/kmlreference#point>`__
(unless heading and/or tilt and/or roll field names are found, in which
case a
`Camera <https://developers.google.com/kml/documentation/kmlreference#camera>`__
object will be generated), LineString to
`<LineString> <https://developers.google.com/kml/documentation/kmlreference#linestring>`__,
LinearRing to
`<LinearRing> <https://developers.google.com/kml/documentation/kmlreference#linearring>`__,
and Polygon to
`<Polygon> <https://developers.google.com/kml/documentation/kmlreference#polygon>`__.
In OGR a polygon contains an array of LinearRings, the first one being
the outer ring. KML has the tags
`<outerBoundaryIs> <https://developers.google.com/kml/documentation/kmlreference#outerboundaryis>`__ and
`<innerBoundaryIs> <https://developers.google.com/kml/documentation/kmlreference#innerboundaryis>`__ to
differentiate between the two. OGR has several Multi types of geometry :
GeometryCollection, MultiPolygon, MultiPoint, and MultiLineString. When
possible, OGR will try to map
`<MultiGeometry> <https://developers.google.com/kml/documentation/kmlreference#multigeometry>`__
to the more precise OGR geometry type (MultiPoint, MultiLineString or
MultiPolygon), and default to GeometryCollection in case of mixed
content.

The following configuration options control geometry translation:

-  .. config:: LIBKML_WRAPDATELINE
      :choices: YES, NO

      Sometimes kml geometry will span the dateline, In applications like QGIS
      or MapServer this will create horizontal lines all the way around the
      globe. Setting this
      to "yes" will cause the libkml driver to split the geometry at the dateline when
      read.

VSI Virtual File System API support
-----------------------------------

The driver supports reading and writing to files managed by VSI Virtual
File System API, which include "regular" files, as well as files in the
/vsizip/ (read-write) , /vsigzip/ (read-write) , /vsicurl/ (read-only)
domains.

Writing to /dev/stdout or /vsistdout/ is also supported.

Examples
--------

The following bash script will build a
:ref:`csv <vector.csv>` file and a
:ref:`vrt <vector.vrt>` file, and then translate them
to KML using :ref:`ogr2ogr` into a .kml
file with timestamps and styling.

::

   #!/bin/bash
   # Copyright (c) 2010, Brian Case
   #
   # Permission is hereby granted, free of charge, to any person obtaining a
   # copy of this software and associated documentation files (the "Software"),
   # to deal in the Software without restriction, including without limitation
   # the rights to use, copy, modify, merge, publish, distribute, sublicense,
   # and/or sell copies of the Software, and to permit persons to whom the
   # Software is furnished to do so, subject to the following conditions:
   #
   # The above copyright notice and this permission notice shall be included
   # in all copies or substantial portions of the Software.
   #
   # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   # OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   # FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   # THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   # LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   # FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   # DEALINGS IN THE SOFTWARE.


   icon="http://maps.google.com/mapfiles/kml/shapes/shaded_dot.png"
   rgba33="#FF9900"
   rgba70="#FFFF00"
   rgba150="#00FF00"
   rgba300="#0000FF"
   rgba500="#9900FF"
   rgba800="#FF0000"

   function docsv {

       IFS=','

       while read Date Time Lat Lon Mag Dep
       do
           ts=$(echo $Date | sed 's:/:-:g')T${Time%%.*}Z
           rgba=""

           if [[ $rgba == "" ]] && [[ $Dep -lt 33 ]]
           then
               rgba=$rgba33
           fi

           if [[ $rgba == "" ]] && [[ $Dep -lt 70 ]]
           then
               rgba=$rgba70
           fi

           if [[ $rgba == "" ]] && [[ $Dep -lt 150 ]]
           then
               rgba=$rgba150
           fi

           if [[ $rgba == "" ]] && [[ $Dep -lt 300 ]]
           then
               rgba=$rgba300
           fi

           if [[ $rgba == "" ]] && [[ $Dep -lt 500 ]]
           then
               rgba=$rgba500
           fi

           if [[ $rgba == "" ]]
           then
               rgba=$rgba800
           fi



           style="\"SYMBOL(s:$Mag,id:\"\"$icon\"\",c:$rgba)\""

           echo $Date,$Time,$Lat,$Lon,$Mag,$Dep,$ts,"$style"
       done

   }


   wget http://neic.usgs.gov/neis/gis/qed.asc -O /dev/stdout |\
    tail -n +2 > qed.asc

   echo Date,TimeUTC,Latitude,Longitude,Magnitude,Depth,timestamp,OGR_STYLE > qed.csv

   docsv < qed.asc >> qed.csv

   cat > qed.vrt << EOF
   <OGRVRTDataSource>
       <OGRVRTLayer name="qed">
           <SrcDataSource>qed.csv</SrcDataSource>
           <GeometryType>wkbPoint</GeometryType>
           <LayerSRS>WGS84</LayerSRS>
           <GeometryField encoding="PointFromColumns" x="Longitude" y="Latitude"/>
       </OGRVRTLayer>
   </OGRVRTDataSource>

   EOF

   ogr2ogr -f libkml qed.kml qed.vrt

The following example shows how the three levels of `<name>`
in LIBKML relate to their controlling options:

.. code-block:: console

    ogr2ogr -f LIBKML /dev/stdout 0.contours.csv \
      -dsco NAME=DSCO -lco NAME=LCO \
      -sql 'SELECT NAME FROM "0.contours"'

    <Document id="root_doc">
      <name>DSCO</name>
      <Document id="_0.contours">
        <name>LCO</name>
        <Placemark id="_0.contours.1">
          <name>-1500</name>
          <LineString>
