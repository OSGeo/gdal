.. _rfc-89:

=============================================================
RFC 89: SQL logging callback
=============================================================

============== =============================================
Author:        Alessandro Pasotti
Contact:       elpaso @ itopen.it
Started:       2022-Nov-30
Status:        Adopted, implemented
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

The callback will provide additional information about the executed
query (if available):

- error string message
- number of affected/retrieved records
- time taken to execute the query


Example API:

.. code-block:: c++

    // Function signature
    typedef void (*GDALQueryLoggerFunc)(const char *pszSQL, const char *pszError, int64_t lNumRecords, int64_t lExecutionTimeMilliseconds, void *pQueryLoggerArg);

    bool CPL_DLL GDALDatasetSetQueryLoggerFunc(GDALDatasetH hDS, GDALQueryLoggerFunc pfnQueryLoggerFunc, void* poQueryLoggerArg );
    
    // C-API
    bool CPL_DLL GDALDatasetSetQueryLoggerFunc(GDALDatasetH hDS, GDALQueryLoggerFunc pfnQueryLoggerFunc, void* poQueryLoggerArg );
        GDALQueryLoggerFunc GDALDatasetQueryLoggerFunc( GDALDatasetH hDS );

    // Function call from the driver
    if ( m_poDS->pfQueryLoggerFunc )
    {
        // -1 for time and num records means no information available
        m_poDS->pfQueryLoggerFunc( soSQL.c_str(), nullptr, elapsedTime, numAffectedRecords, m_poDS->QueryLoggerArg() );
    }


The callback function will be initially used by the SQLite-based drivers only (GPKG included).

The callback may be called by multiple threads in a concurrent way, 
for this reason the implementation of the callback should be robust
to that (use of lock typically). That problem may arise in drivers 
that use multi-threading as an optimization implementation detail. 
And this is typically the case of the GeoPackage driver in its Arrow 
stream interface.

Efficiency considerations
--------------------------

Drivers that support the query logger callback function would need to
check if the function pointer is `nullptr` and call the function if it is
not. The cost of thish check is probably negligible on most architectures.

In order to catch SQLite prepare errors, a prepare function wrapper will be 
called instead of the sqlite3 API C function, this implies the cost of
a function call (which might be inlined if necessary).

Comparison of current master release build with the proposed implementation
on a 145665553 feature 15GB GPKG Point layer stored on fast SSD:

+------------------+---------+-------------------------+                 
|  Benchmark       | Master  | Proposed implementation |
+------------------+---------+-------------------------+                 
| bench_ogr_batch  |   0.13m |                  0.12m  |
+------------------+---------+-------------------------+                 
| bench_ogr_c_capi |   2.57m |                  2.57m  |
+------------------+---------+-------------------------+





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


Voting history
--------------

+1 from PSC members MateuszL, JukkaR, TamasS and EvenR
