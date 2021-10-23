.. _vector.ogdi:

OGDI Vectors
============

.. shortname:: OGDI

.. build_dependencies:: OGDI library

OGDI vector support is optional in OGR, and is normally only configured
if OGDI is installed on the build system. If available OGDI vectors are
supported for read access for the following family types:

-  Point
-  Line
-  Area
-  Text (Currently returned as points with the text in the "text"
   attribute)

OGDI can (among other formats) read VPF products, such as DCW and VMAP.

If an OGDI gltp url is opened directly the OGDI 3.1 capabilities for the
driver/server are queried to get a list of layers. One OGR layer is
created for each OGDI family available for each layer in the datastore.
For drivers such as VRF this can result in a lot of layers. Each of the
layers has an OGR name based on the OGDI name plus an underscore and the
family name. For instance a layer might be called
**watrcrsl@hydro(*)_line** if coming out of the VRF driver.

Setting the *OGR_OGDI_LAUNDER_LAYER_NAMES*
configuration option (or environment variable) to YES causes the layer
names to be simplified. For example : *watrcrsl_hydro* instead of
'watrcrsl@hydro(*)_line'

Alternatively to accessing all the layers in a datastore, it is possible
to open a particular layer using a customized filename consisting of the
regular GLTP URL to which you append the layer name and family type
(separated by colons). This mechanism must be used to access layers of
pre OGDI 3.1 drivers as before OGDI 3.1 there was no regular way to
discover available layers in OGDI.

::

      gltp:[//<hostname>]/<driver_name>/<dataset_name>:<layer_name>:<family>

Where <layer_name> is the OGDI Layer name, and <family> is one of:
"line", "area", "point", or "text".

OGDI coordinate system information is supported for most coordinate
systems. A warning will be produced when a layer is opened if the
coordinate system cannot be translated.

There is no update or creation support in the OGDI driver.

Driver capabilities
-------------------

.. supports_georeferencing::

Error handling
--------------

Starting with GDAL 2.2 and OGDI > 3.2.0beta2, if the OGDI_STOP_ON_ERROR
environment variable is set to NO, some errors can be gracefully
recovered by OGDI (in VPF driver). They will still be caught by GDAL and
emitted as regular GDAL errors.

Note: be aware that this is a work in progress. Not all recoverable
errors can be recovered, and some errors might be recovered silently.

Examples
--------

| Usage example 'ogrinfo':

::

      ogrinfo gltp:/vrf/usr4/mpp1/v0eur/vmaplv0/eurnasia 'watrcrsl@hydro(*)_line'

In the dataset name 'gltp:/vrf/usr4/mpp1/v0eur/vmaplv0/eurnasia' the
gltp:/vrf part is not really in the filesystem, but has to be added. The
VPF data was at /usr4/mpp1/v0eur/. The 'eurnasia' directory should be at
the same level as the dht. and lat. files. The 'hydro' reference is a
subdirectory of 'eurnasia/' where watrcrsl.\* is found.

| Usage examples VMAP0 to SHAPE conversion with 'ogr2ogr':

::

      ogr2ogr watrcrsl.shp gltp:/vrf/usr4/mpp1/v0eur/vmaplv0/eurnasia 'watrcrsl@hydro(*)_line'
      ogr2ogr polbnda.shp  gltp:/vrf/usr4/mpp1/v0eur/vmaplv0/eurnasia 'polbnda@bnd(*)_area'

An OGR SQL query against a VMAP dataset. Again, note the careful quoting
of the layer name.

::

      ogrinfo -ro gltp:/vrf/usr4/mpp1/v0noa/vmaplv0/noamer \
              -sql 'select * from "polbndl@bnd(*)_line" where use=26'

See Also
--------

-  `OGDI.SourceForge.Net <http://ogdi.sourceforge.net/>`__
-  `VMap0
   Coverages <http://www.terragear.org/docs/vmap0/coverage.html>`__
