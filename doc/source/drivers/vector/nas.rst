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
separated strings listed in the configuration option **NAS_INDICATOR** (which defaults
to "NAS-Operationen;AAA-Fachschema;aaa.xsd;aaa-suite") to determine if a
input is a NAS file and ignores all files without any matches.

The configuration option **NAS_GFS_TEMPLATE** makes it possible to cleanly map
element paths to feature attributes using a GFS file like in the GML
driver. Multiple geometries per layer are also possible (eg.
ax_flurstueck.objektkoordinaten next to the regular wkb_geometry).
Starting with GDAL 3.7, defining the **NAS_GFS_TEMPLATE** configuration option is
required for the NAS driver to open a file. It may be set to the empty string
to mean that the driver should try to establish the schema of the file from its
content, but using one of templates mentioned below is recommended.
Alternatively, starting with GDAL 3.10, specifying the ``-if NAS`` option to command line utilities
accepting it, or ``NAS`` as the only value of the ``papszAllowedDrivers`` of
:cpp:func:`GDALOpenEx`, also forces the driver to recognize the passed filename.

The GFS templates and PostgreSQL schemas are part of `norGIS
ALKIS-Import <http://www.norbit.de/68/>`__ (also featuring a shell script and
PyQt frontend which ease the import).  There are currently two versions:

* GeoInfoDok 6: `GFS template <https://github.com/norBIT/alkisimport/blob/master/alkis-schema.gfs>`__ (for GDAL >=3.8 and `<3.8 <https://github.com/norBIT/alkisimport/blob/master/alkis-schema.37.gfs>`__)  and `PostgreSQL schema <https://github.com/norBIT/alkisimport/blob/master/alkis-schema.sql>`__
* GeoInfoDok 7.1.2: `GFS template <https://github.com/norBIT/alkisimport/blob/gid7/alkis-schema.gfs>`__ and `PostgreSQL schema <https://github.com/norBIT/alkisimport/blob/gid7/alkis-schema.sql>`__

The files were generated using `xmi2db <https://github.com/norBIT/xmi2db/>`__ (fork of
`xmi2db <https://github.com/pkorduan/xmi2db>`__) from the official
application schema.

In GDAL 3.8 the creation of the relation layer *alkis_beziehungen* was removed. Prior
the configuration option **NAS_NO_RELATION_LAYER** allowed to disable its
population - which was default in ALKIS-Import. The information found there was
redundant to the relation fields also contained in original elements/tables.
Enabling the option also made progress reporting available.

Duplicate data in datasets will usually causes errors.  When importing separate
datasets into PostgreSQL it is useful to enable :config:`OGR_PG_SKIP_CONFLICTS` to skip
conflicting features.

This driver was implemented within the context of the `PostNAS
project <https://postnas-suite.github.io/postnas-suite/>`__, which has more
information on its use and other related projects.

Driver capabilities
-------------------

.. supports_georeferencing::

.. supports_virtualio::
