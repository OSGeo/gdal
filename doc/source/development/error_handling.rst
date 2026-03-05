Error handling in GDAL
======================

Overview
--------

GDAL provides a centralized error reporting system used across the
C and C++ APIs. Errors are reported using the ``CPLError`` function
and can be retrieved using helper functions such as ``CPLGetLastErrorMsg()``.

Error levels
------------

GDAL defines several error levels:

- ``CE_None`` – No error
- ``CE_Debug`` – Debug message
- ``CE_Warning`` – Warning
- ``CE_Failure`` – Recoverable error
- ``CE_Fatal`` – Fatal error

Reporting errors
----------------

Errors are reported using the ``CPLError`` function.

Example:

.. code-block:: cpp

    CPLError(CE_Failure, CPLE_AppDefined, "Something went wrong");

Retrieving error messages
-------------------------

Applications can retrieve the last error message:

.. code-block:: cpp

    const char* msg = CPLGetLastErrorMsg();

Custom error handlers
---------------------

Applications can install a custom error handler using:

.. code-block:: cpp

    CPLSetErrorHandler();