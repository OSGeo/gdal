.. _vector.gtfs:

GTFS - General Transit Feed Specification
=========================================

.. versionadded:: 3.7

.. shortname:: GTFS

.. built_in_by_default::

This driver can read a .zip archive containing text files following the GTFS
specification.

The driver presents layers with their original structure, and:

- enhances the ``stops`` layer with Point geometries
- enhances the ``trips`` layer with LineString geometries reconstructed by
  joining the ``trips`` layer with ``stops`` and ``stop_times`` layers
- expose a ``shapes_geom`` layer, if the optional ``shapes`` is present by
  constructing LineString geometries.

Open syntax
-----------

The connection name can be:

- a .zip filename containing GTFS .txt files
- a directory name prefixed with ``GTFS:`` (potentially a
  ``/vsizip/path/to/the.zip`` filename prefixed with ``GTFS:``)

Alternatively, starting with GDAL 3.10, specifying the ``-if GTFS`` option to
command line utilities accepting it, or ``GTFS`` as the only value of the
``papszAllowedDrivers`` of :cpp:func:`GDALOpenEx`, also forces the driver to
recognize the passed filename.

Driver capabilities
-------------------

.. supports_virtualio::


Links
-----

-  `GTFS Wikipedia page <https://en.wikipedia.org/wiki/GTFS>`__
