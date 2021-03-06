.. _rfc-54:

=======================================================================================
RFC 54: Dataset transactions
=======================================================================================

Authors: Even Rouault

Contact: even dot rouault at spatialys.com

Status: Adopted, implemented in GDAL 2.0

Summary
-------

This RFC introduces an API to offer a transaction mechanism at dataset
level and uses it in the PostgreSQL, SQLite and GPKG drivers. It also
reworks significantly how transactions are handled in the PostgreSQL
driver. It also introduces a generic mechanism to implement an emulation
of transactions for datasources that would not natively support it, and
uses it in the FileGDB driver.

Rationale
---------

The current abstraction offers a transaction API at the layer level.
However, this is generally misleading since, when it is implemented in
DBMS with BEGIN/COMMIT/ROLLBACK sql statements (PostgreSQL, SQLite,
GPKG, PGDump, MSSQLSpatial), the semantics is really a transaction at
database level that spans over all layers/tables. So even if calling
StartTransaction() on a layer, it also extends on the changes done on
other layers. In a very few drivers
StartTransaction()/CommitTransaction() is sometimes used as a mechanism
to do bulk insertion. This is for example the case of WFS, CartoDB, GFT,
GME. For some of them, it could rather be at dataset level too since
potentially multiple layer modifications could be stacked together.

Furthermode some use cases require updating several layers consistently,
hence the need for a real database level transaction abstraction.

The current situation of various drivers is the following (some of the
below observations resulting from the analysis are kept mainly for the
benefit of developers that would need to work in the drivers) :

PostgreSQL
~~~~~~~~~~

A few facts about cursors used to run GetNextFeature() requests:

-  Cursors are needed for retrieval of huge amount of data without being
   memory bound.
-  Cursors need transactions to run.
-  Default cursors (WITHOUT HOLD) cannot be used outside of the
   transaction that created tem
-  You cannot modify the structure of a table while the transaction
   (yes, the transaction, not the cursor) is still active and if you do
   that on another connection, it hangs until the other connection
   commits or rollbacks)
-  Within a transaction, deleted/modified rows are only visible if they
   are done before declaring the cursor.
-  Cursors WITH HOLD: may be used outside of transaction but cause a
   copy of the table to be done --> bad for performance

Current flaws are :

-  one cannot do interleaved layer reading (beyond the first 500
   features fetched, can be easily seen with OGR_PG_CURSOR_PAGE=1) due
   to the underlying implicit transaction created to read layer A being
   closed when the reading of layer B starts.
-  GetFeature() flushes the current transaction and starts a new one to
   do a cursor SELECT. Which is unnecessary since we retrieve only one
   record
-  SetAttributeFilter() issues a ResetReading() which currently
   FlushSoftTransaction() the ongoing transaction. Can be annoying in a
   scenario with long update where you need transactional guarantee

What works :

-  Transaction support at the layer level forwarded to datasource.
-  Interleaved writing works (even with copy mode)

SQLite/GPKG
~~~~~~~~~~~

-  Mechanisms used to read table content (sqlite3_prepare() /
   sqlite3_step()) do not need transactions.
-  Step sees structure modifications (e.g. column addition) if run after
   prepared statement but before first step.
-  Step sees row modifications/additions as soon as they occur.
-  Transaction support at the layer level forwarded to datasource.

MySQL
~~~~~

-  Cannot do interleaved layer reading (reading in one layer resets the
   other reading) because of the use of mysql_use_result() that can work
   with one single request at a time. mysql_store_result() would be a
   solution but requires ingesting the whole result set into memory,
   which is inpractical for big layers.
-  step does not set row changes once the query has started (if done
   through another connection, because if done through ExecuteSQL() the
   long transaction is interrupted)
-  No transaction support

OCI
~~~

-  Interleaved layer reading works
-  Changes done after SELECT seem not to be seen.
-  No transaction support

FileGDB
~~~~~~~

-  Interleaved layer reading works
-  Changes done after SELECT seem not to be seen.
-  No transaction support

Proposed changes
----------------

GDALDataset changes
~~~~~~~~~~~~~~~~~~~

The following methods are added to GDALDataset (and usable by
OGRDataSource which inherits from GDALDataset).

::

   /************************************************************************/
   /*                           StartTransaction()                         */
   /************************************************************************/

   /**
    \brief For datasources which support transactions, StartTransaction creates a transaction.

    If starting the transaction fails, will return 
    OGRERR_FAILURE. Datasources which do not support transactions will 
    always return OGRERR_UNSUPPORTED_OPERATION.

    Nested transactions are not supported.
    
    All changes done after the start of the transaction are definitely applied in the
    datasource if CommitTransaction() is called. They may be canceled by calling
    RollbackTransaction() instead.
    
    At the time of writing, transactions only apply on vector layers.
    
    Datasets that support transactions will advertise the ODsCTransactions capability.
    Use of transactions at dataset level is generally preferred to transactions at
    layer level, whose scope is rarely limited to the layer from which it was started.
    
    In case StartTransaction() fails, neither CommitTransaction() or RollbackTransaction()
    should be called.
    
    If an error occurs after a successful StartTransaction(), the whole
    transaction may or may not be implicitly canceled, depending on drivers. (e.g.
    the PG driver will cancel it, SQLite/GPKG not). In any case, in the event of an
    error, an explicit call to RollbackTransaction() should be done to keep things balanced.
    
    By default, when bForce is set to FALSE, only "efficient" transactions will be
    attempted. Some drivers may offer an emulation of transactions, but sometimes
    with significant overhead, in which case the user must explicitly allow for such
    an emulation by setting bForce to TRUE. Drivers that offer emulated transactions
    should advertise the ODsCEmulatedTransactions capability (and not ODsCTransactions).
    
    This function is the same as the C function GDALDatasetStartTransaction().

    @param bForce can be set to TRUE if an emulation, possibly slow, of a transaction
                  mechanism is acceptable.

    @return OGRERR_NONE on success.
    @since GDAL 2.0
   */
   OGRErr GDALDataset::StartTransaction(CPL_UNUSED int bForce);


   /************************************************************************/
   /*                           CommitTransaction()                        */
   /************************************************************************/

   /**
    \brief For datasources which support transactions, CommitTransaction commits a transaction.

    If no transaction is active, or the commit fails, will return 
    OGRERR_FAILURE. Datasources which do not support transactions will 
    always return OGRERR_UNSUPPORTED_OPERATION. 
    
    Depending on drivers, this may or may not abort layer sequential readings that
    are active.

    This function is the same as the C function GDALDatasetCommitTransaction().

    @return OGRERR_NONE on success.
    @since GDAL 2.0
   */
   OGRErr GDALDataset::CommitTransaction();

   /************************************************************************/
   /*                           RollbackTransaction()                      */
   /************************************************************************/

   /**
    \brief For datasources which support transactions, RollbackTransaction will roll
    back a datasource to its state before the start of the current transaction. 

    If no transaction is active, or the rollback fails, will return  
    OGRERR_FAILURE. Datasources which do not support transactions will
    always return OGRERR_UNSUPPORTED_OPERATION. 

    This function is the same as the C function GDALDatasetRollbackTransaction().

    @return OGRERR_NONE on success.
    @since GDAL 2.0
   */
   OGRErr GDALDataset::RollbackTransaction();

Note: in the GDALDataset class itself, those methods have an empty
implementation that returns OGRERR_UNSUPPORTED_OPERATION.

Those 3 methods are mapped at the C level as :

::

   OGRErr CPL_DLL GDALDatasetStartTransaction(GDALDatasetH hDS, int bForce);
   OGRErr CPL_DLL GDALDatasetCommitTransaction(GDALDatasetH hDS);
   OGRErr CPL_DLL GDALDatasetRollbackTransaction(GDALDatasetH hDS);

Two news dataset capabilities are added :

-  ODsCTransactions: True if this datasource supports (efficient)
   transactions.
-  ODsCEmulatedTransactions: True if this datasource supports
   transactions through emulation.

Emulated transactions
~~~~~~~~~~~~~~~~~~~~~

A new function OGRCreateEmulatedTransactionDataSourceWrapper() is added
for used by drivers that do not natively support transactions but want
an emulation of them. It could potentially be adopted by any datasource
whose data is supported by files/directories.

::

   /** Returns a new datasource object that adds transactional behavior to an existing datasource.
    * 
    * The provided poTransactionBehaviour object should implement driver-specific
    * behavior for transactions.
    *
    * The generic mechanisms offered by the wrapper class do not cover concurrent
    * updates (though different datasource connections) to the same datasource files.
    *
    * There are restrictions on what can be accomplished. For example it is not
    * allowed to have a unreleased layer returned by ExecuteSQL() before calling
    * StartTransaction(), CommitTransaction() or RollbackTransaction().
    *
    * Layer structural changes are not allowed after StartTransaction() if the
    * layer definition object has been returned previously with GetLayerDefn().
    *
    * @param poBaseDataSource the datasource to which to add transactional behavior.
    * @param poTransactionBehaviour an implementation of the IOGRTransactionBehaviour interface.
    * @param bTakeOwnershipDataSource whether the returned object should own the
    *                                 passed poBaseDataSource (and thus destroy it
    *                                 when it is destroyed itself).
    * @param bTakeOwnershipTransactionBehavior whether the returned object should own
    *                                           the passed poTransactionBehaviour
    *                                           (and thus destroy it when
    *                                           it is destroyed itself).
    * @return a new datasource handle
    * @since GDAL 2.0
    */
   OGRDataSource CPL_DLL* OGRCreateEmulatedTransactionDataSourceWrapper(
                                   OGRDataSource* poBaseDataSource,
                                   IOGRTransactionBehaviour* poTransactionBehaviour,
                                   int bTakeOwnershipDataSource,
                                   int bTakeOwnershipTransactionBehavior);

The definition of the IOGRTransactionBehaviour interface is the
following:

::

   /** IOGRTransactionBehaviour is an interface that a driver must implement
    *  to provide emulation of transactions.
    *
    * @since GDAL 2.0
    */
   class CPL_DLL IOGRTransactionBehaviour
   {
       public:

           /** Start a transaction.
           *
           * The implementation may update the poDSInOut reference by closing
           * and reopening the datasource (or assigning it to NULL in case of error).
           * In which case bOutHasReopenedDS must be set to TRUE.
           *
           * The implementation can for example backup the existing files/directories
           * that compose the current datasource.
           *
           * @param poDSInOut datasource handle that may be modified
           * @param bOutHasReopenedDS output boolean to indicate if datasource has been closed
           * @return OGRERR_NONE in case of success
           */
          virtual OGRErr StartTransaction(OGRDataSource*& poDSInOut,
                                          int& bOutHasReopenedDS) = 0;

           /** Commit a transaction.
           *
           * The implementation may update the poDSInOut reference by closing
           * and reopening the datasource (or assigning it to NULL in case of error).
           * In which case bOutHasReopenedDS must be set to TRUE.
           *
           * The implementation can for example remove the backup it may have done
           * at StartTransaction() time.
           *
           * @param poDSInOut datasource handle that may be modified
           * @param bOutHasReopenedDS output boolean to indicate if datasource has been closed
           * @return OGRERR_NONE in case of success
           */
          virtual OGRErr CommitTransaction(OGRDataSource*& poDSInOut,
                                           int& bOutHasReopenedDS) = 0;

           /** Rollback a transaction.
           *
           * The implementation may update the poDSInOut reference by closing
           * and reopening the datasource (or assigning it to NULL in case of error).
           * In which case bOutHasReopenedDS must be set to TRUE.
           *
           * The implementation can for example restore the backup it may have done
           * at StartTransaction() time.
           *
           * @param poDSInOut datasource handle that may be modified
           * @param bOutHasReopenedDS output boolean to indicate if datasource has been closed
           * @return OGRERR_NONE in case of success
           */
          virtual OGRErr RollbackTransaction(OGRDataSource*& poDSInOut,
                                             int& bOutHasReopenedDS) = 0;
   };

OPGRLayer changes
~~~~~~~~~~~~~~~~~

At the OGRLayer level, the documentation of GetNextFeature() receives
the following additional information to clarify its semantics :

::

   Features returned by GetNextFeature() may or may not be affected by concurrent
   modifications depending on drivers. A guaranteed way of seeing modifications in
   effect is to call ResetReading() on layers where GetNextFeature() has been called,
   before reading again. Structural changes in layers (field addition, deletion, ...)
   when a read is in progress may or may not be possible depending on drivers.
   If a transaction is committed/aborted, the current sequential reading may or may
   not be valid after that operation and a call to ResetReading() might be needed.

PG driver changes
~~~~~~~~~~~~~~~~~

Dataset level transactions have been implemented, and use of implicitly
created transactions reworked.

Interleaved layer reading is now possible.

GetFeature() has been modified to run without a cursor or a transaction,
and all other calls to transactions have been checked/modified to not
reset accidentally a transaction initiated by the user.

Below the new behavior as described in the updated drv_pg_advanced.html
help page :

::

   Efficient sequential reading in PostgreSQL requires to be done within a transaction
   (technically this is a CURSOR WITHOUT HOLD).
   So the PG driver will implicitly open such a transaction if none is currently
   opened as soon as a feature is retrieved. This transaction will be released if
   ResetReading() is called (provided that no other layer is still being read).

   If within such an implicit transaction, an explicit dataset level StartTransaction()
   is issued, the PG driver will use a SAVEPOINT to emulate properly the transaction
   behavior while making the active cursor on the read layer still opened.

   If an explicit transaction is opened with dataset level StartTransaction()
   before reading a layer, this transaction will be used for the cursor that iterates
   over the layer. When explicitly committing or rolling back the transaction, the
   cursor will become invalid, and ResetReading() should be issued again to restart
   reading from the beginning.

   As calling SetAttributeFilter() or SetSpatialFilter() implies an implicit
   ResetReading(), they have the same effect as ResetReading(). That is to say,
   while an implicit transaction is in progress, the transaction will be committed
   (if no other layer is being read), and a new one will be started again at the next
   GetNextFeature() call. On the contrary, if they are called within an explicit
   transaction, the transaction is maintained.

   With the above rules, the below examples show the SQL instructions that are
   run when using the OGR API in different scenarios.


   lyr1->GetNextFeature()             BEGIN (implicit)
                                      DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   lyr1->SetAttributeFilter('xxx')
        --> lyr1->ResetReading()      CLOSE cur1
                                      COMMIT (implicit)

   lyr1->GetNextFeature()             BEGIN (implicit)
                                      DECLARE cur1 CURSOR  FOR SELECT * FROM lyr1 WHERE xxx
                                      FETCH 1 IN cur1

   lyr2->GetNextFeature()             DECLARE cur2 CURSOR  FOR SELECT * FROM lyr2
                                      FETCH 1 IN cur2

   lyr1->GetNextFeature()             FETCH 1 IN cur1

   lyr2->GetNextFeature()             FETCH 1 IN cur2

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   lyr1->SetAttributeFilter('xxx')
        --> lyr1->ResetReading()      CLOSE cur1
                                      COMMIT (implicit)

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR  FOR SELECT * FROM lyr1 WHERE xxx
                                      FETCH 1 IN cur1

   lyr1->ResetReading()               CLOSE cur1

   lyr2->ResetReading()               CLOSE cur2
                                      COMMIT (implicit)

   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   ds->StartTransaction()             BEGIN

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   lyr2->GetNextFeature()             DECLARE cur2 CURSOR FOR SELECT * FROM lyr2
                                      FETCH 1 IN cur2

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   lyr1->SetAttributeFilter('xxx')
        --> lyr1->ResetReading()      CLOSE cur1
                                      COMMIT (implicit)

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR  FOR SELECT * FROM lyr1 WHERE xxx
                                      FETCH 1 IN cur1

   lyr1->ResetReading()               CLOSE cur1

   lyr2->ResetReading()               CLOSE cur2

   ds->CommitTransaction()            COMMIT

   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   ds->StartTransaction()             BEGIN

   lyr1->GetNextFeature()             DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   ds->CommitTransaction()            CLOSE cur1 (implicit)
                                      COMMIT

   lyr1->GetNextFeature()             FETCH 1 IN cur1      ==> Error since the cursor was closed with the commit. Explicit ResetReading() required before

   ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

   lyr1->GetNextFeature()             BEGIN (implicit)
                                      DECLARE cur1 CURSOR FOR SELECT * FROM lyr1
                                      FETCH 1 IN cur1

   ds->StartTransaction()             SAVEPOINT savepoint

   lyr1->CreateFeature(f)             INSERT INTO cur1 ...

   ds->CommitTransaction()            RELEASE SAVEPOINT savepoint

   lyr1->ResetReading()               CLOSE cur1
                                      COMMIT (implicit)


   Note: in reality, the PG drivers fetches 500 features at once. The FETCH 1
   is for clarity of the explanation.

It is recommended to do operations within explicit transactions for ease
of mind (some troubles fixing ogr_pg.py, but which does admittedly weird
things like reopening connections, which does not fly very well with
'implicit' transactions)

GPKG and SQLite driver changes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Dataset level transactions have been implemented. A few fixes made here
and there to avoid resetting accidentally a transaction initiated by the
user.

FileGDB driver changes
~~~~~~~~~~~~~~~~~~~~~~

The FileGDB driver uses the above described emulation to offer a
transaction mechanism. This works by backing up the current state of a
geodatabase when StartTransaction(force=TRUE) is called. If the
transaction is committed, the backup copy is destroyed. If the
transaction is rolled back, the backup copy is restored. So this might
be costly when operating on huge geodatabases. Note that this emulation
has an unspecified behavior in case of concurrent updates (with
different connections in the same or another process).

SWIG bindings (Python / Java / C# / Perl) changes
-------------------------------------------------

The following additions have been done :

-  Dataset.StartTransaction(int force=FALSE)
-  Dataset.CommitTransaction()
-  Dataset.RollbackTransaction()
-  ogr.ODsCTransactions constant
-  ogr.ODsCEmulatedTransactions constant

Utilities
---------

ogr2ogr now uses dataset transactions (instead of layer transactions) if
ODsCTransactions is advertized.

Documentation
-------------

New/modified API are documented. MIGRATION_GUIDE.TXT updated with
mention to below compatibility issues.

Test Suite
----------

The test suite is extended to test

-  updated drivers: PG, GPKG, SQLite, FileGDB
-  use of database transactions by ogr2ogr

Compatibility Issues
--------------------

As described above, subtle behavior changes can be observed with the PG
driver, related to implicit transactions that were flushed before and
are no longer now, but this should hopefully be restricted to
non-typical use cases. So some cases that "worked" before might no
longer work, but the new behavior should hopefully be more
understandable.

The PG and SQLite drivers could accept apparently nested calls to
StartTransaction() (at the layer level). This is no longer possible
since they are now redirected to dataset transactions, that explicitly
do not support it.

Out of scope
------------

The following drivers that implement BEGIN/COMMIT/ROLLBACK could be
later enhanced to support dataset transactions: OCI, MySQL,
MSSQLSpatial.

GFT, CartoDB, WFS could also benefit for dataset transactions.

VRT currently supports layer transactions (if the underlying dataset
support it, and excluding union layers). If dataset transaction were to
be implemented, should it consist in forwarding dataset transaction to
source dataset(s) ? Implementation might be complicated in case the same
dataset is used by multiple sources, but more fundamentally one cannot
guarantee ACID on multiple datasets.

Related tickets
---------------

A proposed revision on how transactions are implemented in the PG driver
was proposed a long time ago
(`https://trac.osgeo.org/gdal/ticket/1265 <https://trac.osgeo.org/gdal/ticket/1265>`__)
to solve some of the above issues. The patch no longer applies but it is
expected that the changes done for this RFC cover the issues that the
ticket wanted to address.

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__), and sponsored by `LINZ (Land
Information New Zealand) <http://www.linz.govt.nz/>`__.

The proposed implementation lies in the "rfc54_dataset_transactions"
branch of the
`https://github.com/rouault/gdal2/tree/rfc54_dataset_transactions <https://github.com/rouault/gdal2/tree/rfc54_dataset_transactions>`__
repository.

The list of changes:
`https://github.com/rouault/gdal2/compare/rfc54_dataset_transactions <https://github.com/rouault/gdal2/compare/rfc54_dataset_transactions>`__

Voting history
--------------

+1 from JukkaR, HowardB and EvenR
