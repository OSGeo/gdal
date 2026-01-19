.. _gdal_vsi_list:

================================================================================
``gdal vsi list``
================================================================================

.. versionadded:: 3.11

.. only:: html

    List files of one of the GDAL Virtual System Interface (VSI)

.. Index:: gdal vsi list

Synopsis
--------

.. program-output:: gdal vsi list --help-doc

Description
-----------

:program:`gdal vsi list` list files of :ref:`virtual_file_systems`.

This is the equivalent of the UNIX ``ls`` command, and ``gdal vsi ls`` is an
alias for ``gdal vsi list``.

By default, it outputs file names, at the immediate level, without details,
and in JSON format.

Options
+++++++

.. option:: --filename <FILENAME>

    Any file name or directory name, of one of the GDAL Virtual file systems.
    Required.

.. option:: -f, --of, --format, --output-format json|text

    Which output format to use. Default is JSON, and starting with GDAL 3.12,
    text when invoked from command line.

.. option:: -l, --long, --long-listing

    Use a long listing format, adding permissions, file size and last modification
    date.

.. option:: -R, --recursive

    List subdirectories recursively. By default the depth is unlimited, but
    it can be reduced with :option:`--depth`.

.. option:: --depth <DEPTH>

    Maximum depth in recursive mode. 1 corresponds to no recursion, 2 to
    the immediate subdirectories, etc.

.. option:: --abs, --absolute-path

    Whether to report file names as absolute paths. By default, they are relative
    to the input file name.

.. option:: --tree

    Use a hierarchical presentation for JSON output, instead of a flat list.
    Only valid when :option:`--output-format` is set to ``json`` (or let at its default value).

Examples
--------

.. example::
   :title: Listing recursively files in /vsis3/bucket with details

   .. code-block:: console

       $ gdal vsi list -lR /vsis3/bucket
