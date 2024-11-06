.. _rfc-104:

===================================================================
RFC 104: Adding a "gdal" front-end command line interface
===================================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault @ spatialys.com
Started:       2024-Nov-06
Status:        Draft
Target:        Presumably GDAL 3.11 for initial version, perhaps with only a subset
============== =============================================

Summary
-------

This RFC introduces a single "gdal" front-end command line interface, with
consistent naming of options.

Motivation
----------

As of 2024, GDAL has 26 years of existence, and throughout the years, various
utilities have been added by diverse contributors, leading to inconsistent
naming of utilities (underscore or not?), options and input/output parameter.
The GDAL User Survey of October-November 2024 shows that a significant proportion
of GDAL users do so through the command line interface, and they suffer from
inconsistencies, hence it is legitimate to enhance their experience.

This RFC adds a new single "gdal" front-end command line interface,
whose sub-commands will match the functionality of existing utilities and re-use
the underlying implementations.

The existing utilities will remain for backwards compatible reasons.
This RFC takes inspiration from `PDAL applications <https://pdal.io/en/2.8.1/apps/index.html>`__
and `rasterio 'rio' utilities <https://rasterio.readthedocs.io/en/stable/api/rasterio.html>`__.

Details
-------

Subcommand syntax
+++++++++++++++++

.. code-block:: shell

        gdal <subcommand> [<options>]... [<positional arguments>]...

where subcommand is something like ``info``, ``convert``, etc.

Option naming conventions
+++++++++++++++++++++++++

* One-letter short names preceded with dash: ``-z``

  When a value is specified, it must be separated with a space: ``-z <value>``

* Longer names preceded with two dashes and using dash to separate words,
  lower-case capitalized: ``--long-name``

  When a single value is expected, it must be separated with a space or equal sign:
  ``--long-name <value>`` or ``--long-name=<value>``.

  When several values are expected, only space can be used:
  ``--bbox <xmin> <ymin> <xmax> <ymax>``
  In the rest of the document, we will use the version with a space separator,
  but equal sign is also accepted.

Repeated values
+++++++++++++++

Existing GDAL command line utilities have an inconsistent strategy regarding
how to specify repeated values (band indices, nodata values, etc.), sometimes
with the switch being repeated many times, sometimes with a single switch but
the values being grouped together and separated with spaces or commas.

For the sake of being friendly with the argument parser we use, we will
standardize on repeating the switch as many times as there are values:
``-z <val1> -z <val2>`` or ``--long-name <value1> --long-name <value2>``.

Specification of input and output files/datasets
+++++++++++++++++++++++++++++++++++++++++++++++++

Two possibilities will be offered:

* positional arguments with input(s) first, output last

  .. code-block:: shell

        gdal <subcommand> <input1> [<input2>]... <output>

* using ``-i / --input`` and ``-o / --output``

  .. code-block:: shell

        gdal <subcommand> -i <input1> [-i <input2>]... -o <output>

Reserved switches
+++++++++++++++++

The following switches are reserved. Meaning that if a subcommand uses them,
it must be with their below semantics and syntax.

* ``-h``, ``--help``: display short help synopsis

* ``--long-usage``: display long help synopsis

* ``--version``: GDAL version

* ``-q``, ``--quiet``: ask for quiet mode (i.e. not stdout messages, no progress bar)

* ``-v``, ``--verbose``: turn on CPL_DEBUG=ON

* ``--debug on|off|<domain>``: turn on debugging

* ``-i <name>``, ``--input <name>``: specify input file/dataset

* ``-o <name>``, ``--output <name>``: specify output file/dataset

* ``-f <format>``, ``--of <format>``: output format. Value is a (not always so)
  "short" driver name: ``GTiff``, ``COG``, ``GPKG``, ``ESRI Shapefile``.
  Also used by ``gdal info`` to select JSON vs text output

* ``--if <format>``: input format. Value is a short driver name.
  Used when autodetection of the appropriate driver fails.

* ``-b <band_number>``, ``--band <band_number>``: specify input raster band number.
  May be repeated for utilities supporting multiple bands

* ``-l <name>``, ``--layer <name>``: specify input vector layer name.
  May be repeated for utilities supporting multiple layers

* ``--co <NAME>=<VALUE>``: driver specific creation option. May be repeated.

* ``--oo <NAME>=<VALUE>``: driver specific open option. May be repeated.

* ``--ot {Byte|UInt16|...}``: output data type (for raster output)

* ``--bbox <xmin> <ymin> <xmax> <ymax>``: as used by ``gdal info vector``,
  ``gdal convert vector``, ``gdal convert raster``

* ``--src-crs <crs_spec>``: TODO: crs or srs... ? Or accept both, with one officially documented and the other one as a hidden option (our argparse framework allows us to that).  Also note the difference of terminology between ``--input`` and ``--src``? Should that be ``--input-crs`` ?

* ``--dst-crs <crs_spec>``: TODO: crs or srs... ? Should that be ``--output-crs`` ?

* ``--override-crs <crs_spec>``: TODO: crs or srs... ?

gdal info
+++++++++

This subcommand will merge together :ref:`gdalinfo`, :ref:`ogrinfo` and :ref:`gdalmdiminfo`.
It will :cpp:func:`GDALDataset::Open` the specified dataset in raster and vector mode.
If the dataset is only a raster one, it will automatically resolve as the sub-subcommand "gdal info raster".
If the dataset is only a vector one, it will automatically resolve as the sub-subcommand as "gdal info vector".

TODO: check that this pipe dream can be actually implemented!!!

In this automated mode, no switch besides open options can be specified, given that we don't know yet in which mode to open.

If the dataset has both raster and vector content, an error will be emitted, inviting the user to specify explicitly the raster or vector mode.

Example:

  .. code-block:: shell

        gdal info my.tif

        gdal info my.gpkg

gdal info raster
++++++++++++++++

Equivalent of existing :ref:`gdalinfo`

Synopsis: ``gdal info raster [-i <filename>] [other options] <filename>``

Example:

  .. code-block:: shell

        gdal info raster my.gpkg

Switches:

* ``-f json|text``, ``--of json|text``: output format. Will default to JSON.

* ``--min-max``

* ``--stats``

* ``--approx-stats``

* ``--hist``

* ``--no-gcp``

* ``--no-md``

* ``--no-ct``

* ``--no-fl``

* ``--no-nodata``

* ``--no-mask``

* ``--checksum``

* ``--list-mdd``

* ``--mdd <domain>|all``

* ``--subdataset <num>``

gdal info vector
++++++++++++++++

Equivalent of existing :ref:`ogrinfo`

Synopsis: ``gdal info vector [-i <filename>] [other options] <filename> [<layername>]...``

Example:

  .. code-block:: shell

        gdal info vector my.gpkg

Switches:

* ``-f json|text``, ``--of json|text``: output format. Will default to JSON.

* ``--sql <statement>``

* ``-l <name>``, ``--layer <name>``

* ``--update``: New default will be read-only

* ``--interleaved-layers``: a.k.a random layer reading mode (ogrinfo ``-al``), for OSM and GMLAS mostly.

* ``--where <statement>``

* ``--dialect <dialectname>``

* ``--bbox <xmin> <ymin> <xmax> <ymax>``

TODO

gdal info multidim
+++++++++++++++++++

Equivalent of existing :ref:`gdalmdiminfo`

* ``-f json``, ``--of json``: output format. Only JSON, as there is currently no text based format.

TODO


gdal convert raster
+++++++++++++++++++

Equivalent of existing :ref:`gdal_translate`

TODO

gdal convert vector
+++++++++++++++++++

Equivalent of existing :ref:`ogr2ogr`

TODO

gdal convert multidim
+++++++++++++++++++++

Equivalent of existing :ref:`gdalmdimtranslate`

TODO

gdal warp
+++++++++

.. note::

    In the User Survey, a number of users have expressed a wish to have
    gdal_translate and gdalwarp functionality merged together. This RFC does not
    attempt at addressing that. Or should it... ? That'd be a huge topic

TODO: warp is also a bit of a misnomer as gdalwarp can mosaic.

Equivalent of existing :ref:`gdalwarp`

TODO

gdal contour
++++++++++++

Equivalent of existing :ref:`gdal_contour`

TODO

gdal rasterize
++++++++++++++

Equivalent of existing :ref:`gdal_rasterize`

TODO

gdal create
+++++++++++

(or ``gdal create raster`` in case we'd have a vector creation one one day?

TODO

gdal footprint
++++++++++++++

Equivalent of existing :ref:`gdal_footprint`

TODO

gdal viewshed
+++++++++++++

Equivalent of existing :ref:`gdal_viewshed`

TODO

gdal dem
++++++++

Equivalent of existing :ref:`gdaldem`

TODO

gdal grid
+++++++++

Equivalent of existing :ref:`gdal_grid`

TODO

gdal build-vrt raster
+++++++++++++++++++++

(TODO: with a dash or not ?)

TODO: or ``gdal virtual raster`` ?

TODO: do we want the raster qualifier as there is a VRT mode in the Python ogrmerge, and so we might want to have a gdal build-vrt vector some day ?

Equivalent of existing :ref:`gdalbuildvrt`

TODO

gdal tile-index raster
++++++++++++++++++++++

(TODO: or tindex?)

Equivalent of existing :ref:`gdaltindex`

TODO

gdal tile-index vector
++++++++++++++++++++++

(TODO: or tindex?)

Equivalent of existing :ref:`ogrtindex`

TODO

gdal clean-border
+++++++++++++++++

(TODO: with a dash or not ?)

Equivalent of existing :ref:`nearblack`

TODO

Implementation details
----------------------

The new ``gdal`` program will map on code of the C API entry points of
existing utilities: :cpp:func:`GDALInfo`, :cpp:func:`GDALVectorInfo`, etc.
No substantial changes in them will be done through this RFC.

Sub commands are an existing capability of the p-ranav/argparser framework
we use since GDAL 3.9: https://github.com/p-ranav/argparse?tab=readme-ov-file#subcommands

TODO: check that sub-sub-commands are handled!

Open questions
--------------

* How to deal with the raster and vector sides when there is ambiguity?

  - "gdal info raster" and "gdal info vector"
  - or "gdal raster info" and "gdal vector info"
  - or "gdal info" and "ogr info" ... ? (but in that logic how to classify contour, rasterize, polygonize that mix both sides?)

* Should we try to port all esoteric existing flags and behaviours? Or just port
  the most useful subset, and defer to existing legacy CLI utilities for the
  esoteric ones. But what to do if we ever drop them someday?

* How to deal with that programmatically... ? Should be have a GDALCommand() C
  API ?

Out of scope
------------

* This RFC only addresses existing C++ utilities. Python utilities that would be
  migrated in the future as C++ utilities should follow this RFC.

* The very specific :ref:`sozip` utility will not follow this RFC. It has been
  design to mimic the existing standard ``zip`` utility.

Backward compatibility
----------------------

Fully backwards compatible. Existing utilities will remain for now, and if they
are decided to be retired, that will likely go through a multi-year deprecation
period.

Testing
-------

Presumably 33% of the whole coding effort

Documentation
-------------

Presumably 33% of the whole coding effort

Team
----

Probably a team effort, at least if we want to have a significant subset
ready for 3.11. Otherwise might take several release cycles. At the very least
we'll need double checking of all naming to avoid adding new inconsistencies!

Related issues and PRs
----------------------

TODO

Voting history
--------------

TBD



.. below is an allow-list for spelling checker.

.. spelling:word-list::
    Subcommand
    subcommand
    multidim
    tindex
    ranav
    argparser
