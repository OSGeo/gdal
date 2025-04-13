.. _gdal_vfs_delete_subcommand:

================================================================================
"gdal vfs delete" sub-command
================================================================================

.. versionadded:: 3.11

.. only:: html

    Delete files located on GDAL Virtual file systems (VSI)

.. Index:: gdal vfs delete

Synopsis
--------

.. program-output:: gdal vfs delete --help-doc

Description
-----------

:program:`gdal vfs delete` delete files and directories located on :ref:`virtual_file_systems`.

This is the equivalent of the UNIX ``rm`` command, and ``gdal vfs rm`` is an
alias for ``gdal vfs delete``.

.. warning::

    Be careful. This command cannot be undone. It can also act on the "real"
    file system.

Options
+++++++

.. option:: -r, -R, --recursive

    Delete directories recursively.

Examples
--------

.. example::
   :title: Delete recursively files from /vsis3/bucket/my_dir

   .. code-block:: console

       $ gdal vfs delete -r /vsis3/bucket/my_dir
