.. _vector.flatgeobuf:

FlatGeobuf
==========

.. versionadded:: 3.1

.. shortname:: ``FlatGeobuf``

Driver capabilities
-------------------

.. supports_create::

.. supports_georeferencing::

.. supports_virtualio::

Open options
------------

-  **VERIFY_BUFFERS=**\ *YES/NO*: Set the YES verify buffers when reading.
    This can provide some protection for invalid/corrupt data with a performance
    trade off. Defaults to YES.

Dataset Creation Options
~~~~~~~~~~~~~~~~~~~~~~~~

None

Layer Creation Options
~~~~~~~~~~~~~~~~~~~~~~

-  **SPATIAL_INDEX=**\ *YES/NO*: Set the YES to create a
   spatial index. Defaults to YES.

VSI Virtual File System API support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The driver supports reading from files managed by VSI Virtual File
System API, which include "regular" files, as well as files in the
/vsizip/, /vsigzip/ , /vsicurl/ domains.


See Also
~~~~~~~~

-  `FlatGeobuf at GitHub <https://github.com/bjornharrtell/flatgeobuf>`__
