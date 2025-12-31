.. _gdal_vector_update:

================================================================================
``gdal vector update``
================================================================================

.. versionadded:: 3.13

.. only:: html

    Update an existing vector dataset with an input vector dataset

.. Index:: gdal vector update

Synopsis
--------

.. program-output:: gdal vector update --help-doc

Description
-----------

:program:`gdal vector update` can be used to update the features of an existing
output vector dataset with the features of the input vector. It identifies
matching features between input and output based on their feature ID by default,
or based on the value of one or several columns forming a key, when using :option:`--key`.

With the default ``merge`` mode, features found in both layers will be updated,
and features only existing in the input layer will be created in the output layer.

The schemas of the input and output layers do not need to be identical
(except on columns specified by :option:`--key`), but they should use the same
CRS for geometry columns, as no on-the-fly reprojection is done. If a column
exists in the input layer but not in the output layer, it will not be created.
And if a column exists in the output layer and not in the input layer, its content
will not be updated.

:program:`gdal vector update` can be used as a
step of a pipeline, with the input dataset being the output of the previous step.

Program-Specific Options
------------------------

.. option:: --input-layer <INPUT-LAYER>

    Input layer name. Must be specified if the input dataset has several layers.

.. option:: --key <KEY>

    Field(s) used as a key to identify features. For multiple fields,
    specify :option:`--key` multiple times.

.. option:: --mode merge|update-only|append-only

    Defines how updates are done. Default is ``merge``.

    When using the default ``merge`` mode, features found in both layers will be updated,
    and features only existing in the input layer will be created in the output layer.

    When using the ``update-only`` mode, features found in both layers will be updated,
    and features only found in the input layer will be ignored.

    When using the ``append-only`` mode, features found in both layers will *not* be updated,
    and features only found in the input layer will be created in the output layer.

.. option:: --output-layer <OUTPUT-LAYER>

    Output layer name. Must be specified if the output dataset has several layers.

Standard Options
----------------

.. collapse:: Details

    .. include:: gdal_options/if.rst

    .. include:: gdal_options/oo.rst

    .. include:: gdal_options/output_oo.rst

Examples
--------

.. example::
   :title: Update existing :file:`out.gpkg` with content of :file:`in.gpkg`, using ``identifier`` as the key.

   .. code-block:: bash

        $ gdal vector update --key identifier in.gpkg out.gpkg
