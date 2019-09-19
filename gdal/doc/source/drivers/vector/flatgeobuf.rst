.. _vector.flatgeobuf:

FlatGeobuf
==========

.. versionadded:: 3.1

.. shortname:: ``FlatGeobuf``

This driver implements read/write support for access to features encoded
in `FlatGeobuf <https://github.com/bjornharrtell/flatgeobuf>`__ format, a
performant binary encoding for geographic data based on flatbuffers that
can hold a collection of Simple Features.

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
------------------------

None

Layer Creation Options
----------------------

-  **SPATIAL_INDEX=**\ *YES/NO*: Set the YES to create a
   spatial index. Defaults to YES.

See Also
--------

-  `FlatGeobuf at GitHub <https://github.com/bjornharrtell/flatgeobuf>`__
