.. _gdal_vsi_delete:

================================================================================
``gdal vsi delete``
================================================================================

.. versionadded:: 3.11

.. only:: html

    Delete files located on GDAL Virtual System Interface (VSI)

.. Index:: gdal vsi delete

Synopsis
--------

.. program-output:: gdal vsi delete --help-doc

Description
-----------

:program:`gdal vsi delete` delete files and directories located on :ref:`virtual_file_systems`.

This is the equivalent of the UNIX ``rm`` command, and ``gdal vsi rm`` is an
alias for ``gdal vsi delete``.

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

       $ gdal vsi delete -r /vsis3/bucket/my_dir
