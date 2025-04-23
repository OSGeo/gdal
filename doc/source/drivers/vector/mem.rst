.. _vector.mem:

================================================================================
MEM -- In Memory datasets
================================================================================

.. versionadded:: 3.11

.. shortname:: MEM

.. built_in_by_default::

GDAL supports the ability to hold datasets in a temporary in-memory
format. This is primarily useful for temporary datasets in scripts or
internal to applications. It is not generally of any use to application
end-users.

This page documents its vector capabilities. Starting with GDAL 3.11, this
driver has been unified with the long-time existing Memory driver to offer
both :ref:`raster <raster.mem>` and vector capabilities.

This driver implements read and write access layers of features
contained entirely in memory. This is primarily useful as a high
performance, and highly malleable working data store. All update
options, geometry types, and field types are supported.

There is no way to open an existing vector MEM dataset. It must be created
with Create(name, 0, 0, 0, GDT_Unknown) and populated and used from that handle.
When the dataset is closed all contents are freed and destroyed.

The driver does not implement spatial or attribute indexing, so spatial
and attribute queries are still evaluated against all features. Fetching
features by feature id should be very fast (just an array lookup and
feature copy).

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

Creation Issues
---------------

Any name may be used for a created dataset. There are no dataset creation
options supported. Layer names need to be unique, but are not otherwise
constrained.

Sparse IDs can be handled when using CreateFeature().

New fields can be added or removed to a layer that already has features, although
the user is responsible for making sure that no ``OGRFeature`` instance is accessible outside
of the driver before changing the layer definition.

Layer creation options
~~~~~~~~~~~~~~~~~~~~~~

|about-layer-creation-options|
The following layer creation options are available:

-  .. lco:: ADVERTIZE_UTF8
      :choices: YES, NO
      :default: NO

      Whether the layer will contain UTF-8 strings.

-  .. lco:: FID
      :choices: <string>
      :default: empty string
      :since: 3.8

      Name of the FID column to create.
