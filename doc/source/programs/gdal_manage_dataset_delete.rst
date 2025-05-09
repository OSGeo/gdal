.. _gdal_manage_dataset_delete:

================================================================================
``gdal manage-dataset delete``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Delete dataset(s).

.. Index:: gdal manage-dataset delete

:program:`gdal manage-dataset delete` delete dataset(s), including potential
side-car/associated files.

Synopsis
--------

.. program-output:: gdal manage-dataset delete --help-doc

Options
-------

.. option:: --filename <FILENAME>

    Any file name or directory name. Required. May be repeated

.. option:: -f, ---format <FORMAT>

    Dataset format. Helps if automatic detection does not work.

Examples
--------

.. example::
   :title: Delete a dataset

   .. code-block:: console

       $ gdal manage-dataset delete NE1_50M_SR_W.tif
