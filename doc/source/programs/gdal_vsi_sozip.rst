.. _gdal_vsi_sozip:

================================================================================
``gdal vsi sozip``
================================================================================

.. versionadded:: 3.11

.. only:: html

    SOZIP (Seek-Optimized ZIP) related commands

.. Index:: gdal vsi sozip

Description
-----------

The :program:`gdal vsi sozip` utility can be used to:

- create a :ref:`sozip_intro` file
- append files to an existing ZIP/SOZip file
- list the contents of a ZIP/SOZip file
- validate a SOZip file
- convert an existing Zip file into a SOZip optimized one

Synopsis
--------

.. program-output:: gdal vsi sozip --help-doc


"gdal vsi sozip create"
-----------------------

Description
+++++++++++

Adds one or several files to a new or existing zip file.

Synopsis
++++++++

.. program-output:: gdal vsi sozip create --help-doc

Options
+++++++

.. option:: -i, --input <INPUT>

    Input filenames. Required. Several file names or directory names accepted.

.. option:: -o, --output <OUTPUT>

    Output ZIP filename. Required. Must have a ``.zip`` extension

.. option:: --overwrite

    Whether overwriting existing output is allowed.

.. option:: -r, --recursive

    Travels the directory structure of the specified directories recursively.

.. option:: -j, --junk-paths, --no-paths

    Store just the name of a saved file (junk the path), and do not store
    directory names. By default, sozip will store the full path (relative to the
    current directory).

.. option:: --enable-sozip auto|yes|no

    In ``auto`` mode, a file is seek-optimized only if its size is above the
    value of :option:`--sozip-min-file-size`.
    In ``yes`` mode, all input files will be seek-optimized.
    In ``no`` mode, no input files will be seek-optimized.

.. option:: --sozip-chunk-size <value>

    Chunk size for a seek-optimized file. Defaults to 32768 bytes. The value
    is specified in bytes, or ``K`` and ``M`` suffix (optionally preceded by a
    space) can be respectively used to specify a value in kilo-bytes or mega-bytes.

.. option:: --sozip-min-file-size <value>

    Minimum file size to decide if a file should be seek-optimized, in
    --enable-sozip=auto mode. Defaults to 1 MB byte. The value
    is specified in bytes, or ``K``, ``M`` or ``G`` suffix (optionally preceded by a
    space) can be respectively used to specify a value in kilo-bytes, mega-bytes
    or giga-bytes.

.. option:: --content-type <value>

    Store the Content-Type for the file being added as a key-value pair in the
    extra field extension 'KV' (0x564b) dedicated to storing key-value pair metadata

.. option:: -q, --quiet

    Do not output any informative message (only errors).

Multithreading
++++++++++++++

The :config:`GDAL_NUM_THREADS` configuration option can be set to
``ALL_CPUS`` or a integer value to specify the number of threads to use for
SOZip-compressed files. Defaults to ``ALL_CPUS``.

Examples
++++++++

.. example::
   :title: Create a, potentially seek-optimized, ZIP file with the content of my.gpkg

   .. code-block:: bash

        gdal vsi sozip create my.gpkg my.gpkg.zip

.. example::
   :title: Create a, potentially seek-optimized, ZIP file from the content of a source directory:

   .. code-block:: bash

       gdal vsi sozip create -r source_dir/ my.gpkg.zip


"gdal vsi sozip optimize"
-------------------------

Description
+++++++++++

Create a new zip file from the content of an existing one, possibly applying
SOZip optimization when relevant.

Synopsis
++++++++

.. program-output:: gdal vsi sozip optimize --help-doc

Options
+++++++

.. option:: -i, --input <INPUT>

    Input ZIP filename. Required.

.. option:: -o, --output <OUTPUT>

    Output ZIP filename. Required. Must have a ``.zip`` extension

.. option:: --overwrite

    Whether overwriting existing output is allowed.

.. option:: --enable-sozip auto|yes|no

    In ``auto`` mode, a file is seek-optimized only if its size is above the
    value of :option:`--sozip-chunk-size`.
    In ``yes`` mode, all input files will be seek-optimized.
    In ``no`` mode, no input files will be seek-optimized.

.. option:: --sozip-chunk-size <value>

    Chunk size for a seek-optimized file. Defaults to 32768 bytes. The value
    is specified in bytes, or K and M suffix can be respectively used to
    specify a value in kilo-bytes or mega-bytes.

.. option:: --sozip-min-file-size <value>

    Minimum file size to decide if a file should be seek-optimized, in
    --enable-sozip=auto mode. Defaults to 1 MB byte. The value
    is specified in bytes, or K, M or G suffix can be respectively used to
    specify a value in kilo-bytes, mega-bytes or giga-bytes.

.. option:: -q, --quiet

    Do not output any informative message (only errors).

Multithreading
++++++++++++++

The :config:`GDAL_NUM_THREADS` configuration option can be set to
``ALL_CPUS`` or a integer value to specify the number of threads to use for
SOZip-compressed files. Defaults to ``ALL_CPUS``.

Examples
++++++++

.. example::
   :title: Create a, potentially seek-optimized, ZIP file ``sozip_optimized.zip`` from an existing ZIP file ``in.zip``.

   .. code-block:: bash

        gdal vsi sozip optimize in.zip sozip_optimized.zip


"gdal vsi sozip list"
---------------------

Description
+++++++++++

List the files contained in the zip file in an output similar to Info-ZIP
:program:`unzip` utility, but with the addition of a column indicating
whether each file is seek-optimized.

Synopsis
++++++++

.. program-output:: gdal vsi sozip list --help-doc

Options
+++++++

.. option:: -i, --input <INPUT>

    Input ZIP filename. Required.

Examples
++++++++

.. example::
   :title: List contents of ``my.zip``.

   .. code-block:: bash

        gdal vsi sozip list my.zip


"gdal vsi sozip validate"
-------------------------

Description
+++++++++++

Validates a SOZip file. Baseline ZIP validation is done in a light way,
limited to being able to browse through ZIP records with the InfoZIP-based
ZIP reader used by GDAL. But validation of the SOZip-specific aspects is
done more thoroughly.

Synopsis
++++++++

.. program-output:: gdal vsi sozip list --help-doc

Options
+++++++

.. option:: -i, --input <INPUT>

    Input ZIP filename. Required.

.. option:: -q, --quiet

    Do not output any informative message (only errors).

.. option:: -v, --verbose

    Turn on verbose mode.

Examples
++++++++

.. example::
   :title: Validate ``my.zip``.

   .. code-block:: bash

        gdal vsi sozip validate my.zip
