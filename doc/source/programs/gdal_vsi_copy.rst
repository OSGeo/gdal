.. _gdal_vsi_copy:

================================================================================
``gdal vsi copy``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Copy files located on GDAL Virtual System Interface (VSI)

.. Index:: gdal vsi copy

Synopsis
--------

.. program-output:: gdal vsi copy --help-doc

Description
-----------

:program:`gdal vsi copy` copy files and directories located on :ref:`virtual_file_systems`.

It can copy files and directories between different virtual file systems.

This is the equivalent of the UNIX ``cp`` command, and ``gdal vsi cp`` is an
alias for ``gdal vsi copy``.

Options
+++++++

.. option:: -r, --recursive

    Copy directories recursively.

.. option:: --skip-errors

    Skip errors that occur while while copying.

Examples
--------

.. example::
   :title: Copy recursively files from /vsis3/bucket/my_dir to local directory, creating a my_dir directory if it does not exist.

   .. code-block:: console

       $ gdal vsi copy -r /vsis3/bucket/my_dir .

.. example::
   :title: Copy recursively files from /vsis3/bucket/my_dir to local directory, *without* creating a my_dir directory, without progress bar

   .. code-block:: console

       $ gdal vsi copy --quiet -r /vsis3/bucket/my_dir/* .
