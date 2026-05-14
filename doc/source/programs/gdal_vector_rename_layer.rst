.. _gdal_vector_rename_layer:

================================================================================
``gdal vector rename-layer``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Rename layer(s) of a vector dataset.

.. Index:: gdal vector rename-layer

Synopsis
--------

.. program-output:: gdal vector rename-layer --help-doc

Description
-----------

:program:`gdal vector rename-layer` can be used to rename layer(s).

It has two modes:

- one where the user renames a single layer to a given name with :option:`--output-layer`,
  combined with :option:`--input-layer` if there are more than one layer.

- another one where the user may force various operations to alter the layer names:

  * Force characters to ASCII:  :option:`--ascii`

  * Force characters to lower case:  :option:`--lower-case`

  * Force compatibility with file names:  :option:`--filename-compatible`

  * Remove or replace characters from a list of reserved characters:  :option:`--reserved-characters`

  * Truncate to a maximum number of characters:  :option:`--max-length`

  For :option:`--filename-compatible` and :option:`--reserved-characters`,
  incompatible characters are removed, unless :option:`--replacement-character`
  is specified.

  In this mode, uniqueness of layer names is also attempted by appending ``_{N}``
  where NN is a sequence number. Uniqueness may not be achieved if
  :option:`--max-length` is set to a too low value.

This subcommand is also available as a potential step of :ref:`gdal_vector_pipeline`

Program Specific Options
------------------------

.. option:: --input-layer <INPUT-LAYER>

    Name of input layer. To be used together with :option:`--output-layer`

.. option:: --output-layer <OUTPUT-LAYER>

    Name of output layer. If there are several input layers,
    :option:`--output-layer` must also be specified.

.. option:: --ascii

    Force characters to ASCII. Currently only accented characters characters in
    `Latin 1 Supplement <https://en.wikipedia.org/wiki/Latin-1_Supplement>`__
    and `Latin Extended A <https://en.wikipedia.org/wiki/Latin_Extended-A>`__
    character set have a sensible ASCII replacement. Other non-ASCII characters
    will be removed or replaced by the replacement character specified with
    :option:`--replacement-character`

.. option:: --lower-case

    Force characters to lower-case. Currently only ASCII upper case characters
    are replaced. This option is applied after :option:`--ascii`, if the later
    is specified.

.. option:: --filename-compatible

    Force layer names to be compatible of file names for Linux, Windows or MacOS,
    independently from the operating system on which this command is run.

.. option:: --reserved-characters <RESERVED-CHARACTERS>

   Reserved character(s) to remove or replace with the replacement character
   specified with :option:`--replacement-character`

.. option:: --replacement-character <REPLACEMENT-CHARACTER>

   Replacement character when ASCII conversion is not possible or to substitute
   to reserved characters.

.. option:: --max-length <MAX-LENGTH>

   Truncate to a maximum number of characters

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/append_vector.rst

    .. include:: gdal_options/co_vector.rst

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/lco.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/of_vector.rst

    .. include:: gdal_options/output_layer.rst

    .. include:: gdal_options/output_oo.rst

    .. include:: gdal_options/overwrite.rst

    .. include:: gdal_options/overwrite_layer.rst

    .. include:: gdal_options/skip_errors.rst

    .. include:: gdal_options/update.rst

    .. include:: gdal_options/upsert.rst

.. Return status code
.. ------------------

.. include:: return_code.rst

Examples
--------

.. example::
   :title: Rename a given layer to a user selected name.

   .. code-block:: console

       $ gdal vector rename-layer input.gpkg output.gpkg --input-layer "name with space" --output-layer "without_space"

.. example::
   :title: Force layer names to ASCII, lower case, without space character and to be filename compatible.

   .. code-block:: console

       $ gdal vector rename-layer input.gpkg output_shp_directory --output-format "ESRI Shapefile" --ascii --lower-case --filename-compatible --reserved-characters " "
