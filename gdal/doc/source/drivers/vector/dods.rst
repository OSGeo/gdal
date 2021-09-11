.. _vector.dods:

DODS/OPeNDAP
============

.. shortname:: DODS

.. build_dependencies:: libdap

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_OGR_DODS

This driver implements read-only support for reading feature data from
OPeNDAP (DODS) servers. It is optionally included in OGR if built with
OPeNDAP support libraries.

When opening a database, its name should be specified in the form
"DODS:url". The URL may include a constraint expression a shown here.
Note that it may be necessary to quote or otherwise protect DODS URLs on
the commandline if they include question mark or ampersand characters as
these often have special meaning to command shells.

::

   DODS:http://dods.gso.uri.edu/dods-3.4/nph-dods/broad1999?&press=148

By default top level Sequence, Grid and Array objects will be translated
into corresponding layers. Sequences are (by default) treated as point
layers with the point geometries picked up from lat and lon variables if
available. To provide more sophisticated translation of sequence, grid
or array items into features it is necessary to provide additional
information to OGR as DAS (dataset auxiliary information) either from
the remote server, or locally via the AIS mechanism.

A DAS definition for an OGR layer might look something like:

::

   Attributes {
       ogr_layer_info {
       string layer_name WaterQuality;
       string spatial_ref WGS84;
       string target_container Water_Quality;
           layer_extents {
           Float64 x_min -180;
           Float64 y_min -90;
           Float64 x_max 180;
           Float64 y_max 90;
           }
           x_field {
               string name YR;
           string scope dds;
           }
           y_field {
               string name JD;
           string scope dds;
           }
       }
   }

Driver capabilities
-------------------

.. supports_georeferencing::

Caveats
-------

-  No field widths are captured for attribute fields from DODS.
-  Performance for repeated requests is dramatically improved by
   enabling DODS caching. Try setting USE_CACHE=1 in your ~/.dodsrc.

See Also
--------

-  `OPeNDAP <http://www.opendap.org/>`__
