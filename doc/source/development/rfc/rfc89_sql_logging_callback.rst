.. _rfc-89:

=============================================================
RFC 89: SQL logging callback
=============================================================

============== =============================================
Author:        Alessandro Pasotti
Contact:       elpaso @ itopen.it
Started:       2022-Nov-30
Status:        Draft
Target:        GDAL 3.7
============== =============================================

Summary
-------

The document proposes and describes the introduction of a new
callback function to allow applications that use the GDAL library
to monitor the SQL queries actually sent from GDAL to the database.

Motivation
----------

Applications (for example QGIS) may provide a debugging panel that
allow users to monitor the SQL command that are sent to the layers
stored on databases.

This panel implemented in QGIS is very useful to identify erroneous
or inefficient queries and also made the users more conscious about
what is going on in the application when layers are loaded and features
are fetched.


Technical details
-----------------

The implementation details are still to be defined, a possible
implementation would provide a method to `GDALDataset` that
allows client code to set the callback function (with some opaque
context to be passed back to client code).

The callback function will then be called from the drivers that
make database calls each time a SQL command is sent to the backend,
in case of prepared queries the actual SQL after parameter
substitutions should be sent.

The callback may provide additional information about the executed
query:

- error string message
- number of affected/retrieved records

Further research is necessary to determine if the following 
information could be also provided:

- time taken to execute the query

Example API:

.. code-block:: c++

    // Function signature
    typedef int (*GDALQueryLoggerFunc)(const char *pszSQL, const char *pszError, int64_t llNumRecords, void *pQueryLoggerArg);

    // C-API
    bool CPL_DLL GDALDatasetSetQueryLoggerFunc(GDALDatasetH hDS, GDALQueryLoggerFunc pfnQueryLoggerFunc, void* poQueryLoggerArg );
        GDALQueryLoggerFunc GDALDatasetQueryLoggerFunc( GDALDatasetH hDS );

    // Function call from the driver
    if ( m_poDS->pfQueryLoggerFunc )
    {
        m_poDS->pfQueryLoggerFunc( soSQL.c_str(), nullptr, -1, m_poDS->QueryLoggerArg() );
    }


The callback function will be initially used by the GeoPackage driver only.


Efficiency considerations
--------------------------

Drivers that support the query logger callback function would need to
check if the function pointer is `nullptr` and call the function if it is
not.

The cost of the a.m. check is probably negligible on most architectures.


Backward compatibility
----------------------

None.

The callback would be set from a new method, no changes
to the class constructor would be needed.

SWIG Bindings
-------------

This implementation will not be exposed to bindings.

Testing
-------

A C++ test will be added to the test suite.

Related tickets and PRs:
------------------------

TBD

Voting history
--------------


