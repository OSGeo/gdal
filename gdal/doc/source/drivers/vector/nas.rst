.. _vector.nas:

NAS - ALKIS
===========

.. shortname:: NAS

.. build_dependencies:: Xerces

The NAS driver reads the NAS/ALKIS format used for cadastral data in
Germany. The format is a GML profile with fairly complex GML3 objects
not easily read with the general OGR GML driver.

This driver depends on GDAL/OGR being built with the Xerces XML parsing
library.

The driver looks for "opengis.net/gml" and one of the strings semicolon
separated strings listed in the option **NAS_INDICATOR** (which defaults
to "NAS-Operationen;AAA-Fachschema;aaa.xsd;aaa-suite") to determine if a
input is a NAS file and ignores all files without any matches.

In GDAL 2.3 a bunch of workarounds were removed, that caused the driver
to remap or ignore some elements and attributes internally to avoid
attribute conflicts (e.g. *zeigtAufExternes*). Instead it now takes the
**NAS_GFS_TEMPLATE** option, that makes it possible to cleanly map
element paths to feature attributes using a GFS file like in the GML
driver. Multiple geometries per layer are also possible (eg.
ax_flurstueck.objektkoordinaten next to the regular wkb_geometry).

A `GFS
template <https://github.com/norBIT/alkisimport/blob/master/alkis-schema.gfs>`__
and a corresponding `PostgreSQL
schema <https://github.com/norBIT/alkisimport/blob/master/alkis-schema.sql>`__
of the full NAS schema are part of `norGIS
ALKIS-Import <http://www.norbit.de/68/>`__ (also featuring a shell
script and PyQt frontend which ease the import). The two files were
generated using `xmi2db <https://github.com/norBIT/xmi2db/>`__ (fork of
`xmi2db <https://github.com/pkorduan/xmi2db>`__) from the official
application schema.

New in 2.3 is also the option **NAS_NO_RELATION_LAYER** that allows
disabling populating the table *alkis_beziehungen*. The information found
there is redundant to the relation fields also contained in original
elements/tables. Enabling the option also makes progress reporting
available.

This driver was implemented within the context of the `PostNAS
project <http://trac.wheregroup.com/PostNAS>`__, which has more
information on its use and other related projects.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
