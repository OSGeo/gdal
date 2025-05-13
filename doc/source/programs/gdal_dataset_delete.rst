.. _gdal_dataset_delete:

================================================================================
``gdal dataset delete``
================================================================================

.. versionadded:: 3.12

.. only:: html

    Delete dataset(s).

.. Index:: gdal dataset delete

:program:`gdal dataset delete` delete dataset(s), including potential
side-car/associated files.

Synopsis
--------

.. program-output:: gdal dataset delete --help-doc

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

       $ gdal dataset delete NE1_50M_SR_W.tif
