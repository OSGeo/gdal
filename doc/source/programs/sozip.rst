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

    sozip [--quiet|--verbose]
          [[-g|--grow] | [--overwrite]]
          [-r|--recurse-paths]
          [-j|--junk-paths]
          [-l|--list]
          [--enable-sozip=auto/yes/no]
          [--sozip-chunk-size=value]
          [--sozip-min-file-size=value]
          zip_filename [filename]*


Description
-----------

The :program:`sozip` utility can be used to create a seek-optimized ZIP file,
append files to an existing ZIP file or list the contents of a ZIP file.

.. program:: sozip

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

.. option:: -j
.. option:: --junk-paths

    Store just the name of a saved file (junk the path), and do not store
    directory names. By default, sozip will store the full path (relative to the
    current directory).

.. option:: --enable-sozip=auto/yes/no

    In ``auto`` mode, a file is seek-optimized only if its size is above the
    value of :option:`--sozip-chunk-size`.
    In ``yes`` mode, all input files will be seek-optimized.
    In ``no`` mode, no input files will be seek-optimized.

.. option:: --sozip-chunk-size

    Chunk size for a seek-optimized file. Defaults to 32768 bytes. The value
    is specified in bytes, or K and M suffix can be respecively used to
    specify a value in kilo-bytes or mega-bytes.

.. option:: --sozip-min-file-size

    Minimum file size to decide if a file should be seek-optimized, in
    --enable-sozip=auto mode. Defaults to 1 MB byte. The value
    is specified in bytes, or K, M or G suffix can be respecively used to
    specify a value in kilo-bytes, mega-bytes or giga-bytes.

.. option:: <zip_filename>

    Filename of the zip file to create/append to/list.

.. option:: <filename>

    Filename of the file to add.


Multithreading
--------------

The :decl_configoption:`GDAL_NUM_THREADS` configuration option can be set to
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


List the contents of a ZIP file and display which files are seek-optimized:

::

    sozip -l my.gpkg.zip
