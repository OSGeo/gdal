.. _vector_common_options:

================================================================================
Common options for vector programs
================================================================================

All GDAL OGR command line programs support the following common options.

.. option:: --version

    Report the version of GDAL and exit.

.. option:: --build

    Report detailed information about GDAL in use.

.. option:: --formats

    List all vector formats supported by this GDAL build (read-only and
    read-write) and exit. The format support is indicated as follows:
    ``ro`` is read-only driver; ``rw`` is read or write (i.e. supports
    :cpp:func:`CreateCopy`); ``rw+`` is read, write and update (i.e. supports
    Create). A ``v`` is appended for formats supporting virtual IO
    (``/vsimem``, ``/vsigzip``, ``/vsizip``, etc). A ``s`` is appended for
    formats supporting subdatasets.

.. option:: --format <format>

    List detailed information about a single format driver.
    The format should be the short name reported in the :option:`--formats`
    list, such as GML.

.. option:: --optfile <filename>

    Read the named file and substitute the contents into the command line
    options list. Lines beginning with ``#`` will be ignored.
    Multi-word arguments may be kept together with double quotes.

.. option:: --config <key> <value>

    Sets the named configuration keyword to the given value, as opposed to
    setting them as environment variables. Some common configuration keywords
    are SHAPE_ENCODING (force shapefile driver to read DBF files with the given
    character encoding) and CPL_TEMPDIR (define the location of temporary files).
    Individual drivers may be influenced by other :ref:`configuration options <list_config_options>`.

.. option:: --debug <value>

    Control what debugging messages are emitted.
    A value of ON will enable all debug messages.
    A value of OFF will disable all debug messages.
    Otherwise, a debug messege will be emitted if its
    category appears somewhere in the value string.

.. option:: --help-general

    Gives a brief usage message for the generic GDAL OGR command line options and exit.
