.. _sozip:

================================================================================
sozip
================================================================================

.. versionadded:: 3.7

.. only:: html

    Generate a seek-optimized ZIP (SOZip) file.

.. Index:: sozip

Synopsis
--------

.. code-block::

    sozip [--help] [--help-general]
          [--quiet|--verbose]
          [[-g|--grow] | [--overwrite]]
          [-r|--recurse-paths]
          [-j|--junk-paths]
          [-l|--list]
          [--optimize-from=<input.zip>]
          [--validate]
          [--enable-sozip={auto|yes|no}]
          [--sozip-chunk-size=<value>]
          [--sozip-min-file-size=<value>]
          [--content-type=<value>]
          <zip_filename> [<filename>]...


Description
-----------

The :program:`sozip` utility can be used to:

- create a :ref:`sozip_intro` file
- append files to an existing ZIP/SOZip file
- list the contents of a ZIP/SOZip file
- validate a SOZip file
- convert an existing Zip file in a SOZip optimized one


.. program:: sozip

.. include:: options/help_and_help_general.rst

.. option:: --quiet

    Quiet mode. No progress message is emitted on the standard output.

.. option:: --verbose

    Verbose mode.

.. option:: -g
.. option:: --grow

    Grow an existing zip file with the content of the specified filename(s).
    This is the default mode of the utility. This switch is here for
    compatibility with Info-ZIP :program:`zip` utility

.. option:: --overwrite

    Overwrite the target zip file if it already exists.

.. option:: -l
.. option:: --list

    List the files contained in the zip file in an output similar to Info-ZIP
    :program:`unzip` utility, but with the addition of a column indicating
    whether each file is seek-optimized.

.. option:: --validate

    Validates a SOZip file. Baseline ZIP validation is done in a light way,
    limited to being able to browse through ZIP records with the InfoZIP-based
    ZIP reader used by GDAL. But validation of the SOZip-specific aspects is
    done in a more thorougful way.

.. option:: -r
.. option:: --recurse-paths

    Travels the directory structure of the specified directory/directories recursively.

.. option:: -j
.. option:: --junk-paths

    Store just the name of a saved file (junk the path), and do not store
    directory names. By default, sozip will store the full path (relative to the
    current directory).

.. option:: --optimize-from=<input.zip>

    Re-process {input.zip} to generate a SOZip-optimized .zip. Options
    :option:`--enable-sozip`, :option:`--sozip-chunk-size` and
    :option:`--sozip-min-file-size` may be used in that mode.

.. option:: --enable-sozip={auto|yes|no}

    In ``auto`` mode, a file is seek-optimized only if its size is above the
    value of :option:`--sozip-chunk-size`.
    In ``yes`` mode, all input files will be seek-optimized.
    In ``no`` mode, no input files will be seek-optimized.

.. option:: --sozip-chunk-size=<value>

    Chunk size for a seek-optimized file. Defaults to 32768 bytes. The value
    is specified in bytes, or K and M suffix can be respectively used to
    specify a value in kilo-bytes or mega-bytes.

.. option:: --sozip-min-file-size=<value>

    Minimum file size to decide if a file should be seek-optimized, in
    --enable-sozip=auto mode. Defaults to 1 MB byte. The value
    is specified in bytes, or K, M or G suffix can be respectively used to
    specify a value in kilo-bytes, mega-bytes or giga-bytes.

.. option:: --content-type=<value>

    Store the Content-Type for the file being added as a key-value pair in the
    extra field extension 'KV' (0x564b) dedicated to storing key-value pair metadata

.. option:: <zip_filename>

    Filename of the zip file to create/append to/list.

.. option:: <filename>

    Filename of the file to add.


Multithreading
--------------

The :config:`GDAL_NUM_THREADS` configuration option can be set to
``ALL_CPUS`` or a integer value to specify the number of threads to use for
SOZip-compressed files. Defaults to ``ALL_CPUS``.

C API
-----

Functionality of this utility can be done from C with :cpp:func:`CPLAddFileInZip`
or :cpp:func:`VSICopyFile`.

Examples
--------

Create a, potentially seek-optimized, ZIP file with the content of my.gpkg:

::

    sozip my.gpkg.zip my.gpkg


Create a, potentially seek-optimized, ZIP file from the content of a source
directory:

::

    sozip -r my.gpkg.zip source_dir/


Create a, potentially seek-optimized, ZIP file from an existing ZIP file.

::

    sozip --convert-from=in.zip out.zip


List the contents of a ZIP file and display which files are seek-optimized:

::

    sozip -l my.gpkg.zip


Validates a SOZip file:

::

    sozip --validate my.gpkg.zip
