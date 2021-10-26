.. _rfc-47:

=======================================================================================
RFC 47: Per Dataset Caching and GDALRasterBand Multithreading (not implemented)
=======================================================================================

Author: Blake Thompson

Contact: flippmoke at gmail dot com

Status: Development

Summary
-------

When utilizing GDAL in multithread code, it was found that often the
limiting portion of the code was often the LRU block cache within GDAL.
This is an attempt to make the LRU cache more efficient in multithreaded
situations by making it possible to have multiple LRU per dataset and
optimizing when locking occurs. Additionally, the changes outlined
attempt to find an efficient manner to manage data within the cache.

*This change attempts to:*

-  Make the caching system within raster datasets:

   -  Thread Safe
   -  Provide performance more linearly with an increasing number of
      threads

-  Reduce the scope of the current cache locking.
-  Optionally enable a per dataset cache (rather then a global cache)
-  Make Mem datasets READ thread safe per dataset.
-  Lay the ground work for future development to increase thread safety
   in drivers.

*This change does NOT attempt to:*

-  Make all drivers thread safe
-  Make datasets thread safe

Two Different Solutions
-----------------------

Two different ways for solving this problem are being proposed and both
have been coded up (test code for each still to be written). However,
both share some common solutions. First I will go over the common
changes for the two different methods, then the ways in which the two
solutions differ.

Pull Requests
-------------

-  `Pull Request #1 <https://github.com/OSGeo/gdal/pull/38>`__ -
   SOLUTION 1 (Dataset RW Locking)
-  `Pull Request #2 <https://github.com/OSGeo/gdal/pull/39>`__ -
   SOLUTION 2 (Block RW Locking)

Common Solution
---------------

Dataset Caching
~~~~~~~~~~~~~~~

The static global mutex that is limiting performance is located within
gcore/gdalrasterblock.cpp. This mutex is there to protect the setting of
the maximum cache, the LRU cache itself itself, and the current size of
the cache. The current scope of this mutex makes it lock for extended
periods once the cache is full, and new memory is being initialized in
GDALRasterBlock::Internalize().

In order to remove the need for this LRU cache to be locked more often a
new global config option is introducted "GDAL_DATASET_CACHING". This
causes the LRU cache to be per dataset when set to "YES", rather then a
global cache ("NO" Default). Doing this will also allow threaded
applications to flush only the cache for a single dataset, improving
performance in some situations for two reasons. First a cache of a more
commonly used dataset, might be set separately from other datasets,
meaning that it is more likely to remain cached. The second is that the
lack of a common global mutex will result in a less likely situation of
two threads locking the same mutex if operations are being performed on
different datasets.

In order to have management of the different caches, a
GDALRasterBlockManager class is introduced. This class is responsible
for the management of the cache in the global or per dataset situations.

GDALRasterBlockManager
^^^^^^^^^^^^^^^^^^^^^^

::


   class CPL_DLL GDALRasterBlockManager
   {
       friend class GDALRasterBlock;
       
       int             bCacheMaxInitialized;
       GIntBig         nCacheMax;
       volatile GIntBig nCacheUsed;
       volatile GDALRasterBlock *poOldest;    /* tail */
       volatile GDALRasterBlock *poNewest;    /* head */
       void            *hRBMMutex;

     public:
                   GDALRasterBlockManager();
       virtual     ~GDALRasterBlockManager();
       void        SetCacheMax( GIntBig nBytes );
       GIntBig     GetCacheMax(void);
       GIntBig     GetCacheUsed(void);
       int         FlushCacheBlock(void);
       void        FlushTillBelow();
       void        Verify();
       int         SafeLockBlock( GDALRasterBlock ** );
       void        DestroyRBMMutex();
   };

Many of the operations originally done by statistics:* within GDALRasterBlock
are now moved into the GDALRasterBlockManager.

GDALDataset
^^^^^^^^^^^

Every GDALDataset now has a:

::

   GDALRasterBlockManager *poRasterBlockManager;

This is set at initialization of the dataset via:

::

   bDatasetCache =  CSLTestBoolean( 
   CPLGetConfigOption( "GDAL_DATASET_CACHING", "NO") );

   if ( bDatasetCache ) 
   {    
       poRasterBlockManager = new GDALRasterBlockManager();
   }
   else
   {   
       poRasterBlockManager = GetGDALRasterBlockManager();
   }

GDALRasterBand
^^^^^^^^^^^^^^

In order to make caching safer and more efficient, a mutex as also
introduced in GDALRasterBand as well. The job of this mutex is to
protect the RasterBlock array per band (papoBlocks).

Thread Safety and the Two Solutions
-----------------------------------

The multithreading of GDAL is a complicated thing, while these changes
do seek to *improve* threading within GDAL. It does not *solve*
threading problems within GDAL and make it truly thread safe. The goal
of this change is simply to make the cache thread safe, in order to
achieve this three mutexes are utilized. Where these three mutexes are
located is different between the two solutions proposed.

.. _solution-1-rw-mutex-in-gdaldataset-:

Solution 1 (RW Mutex in GDALDataset )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mutexes
^^^^^^^

For solution 1 the three mutexes are:

-  Dataset RW Mutex (per GDALDataset)
-  Band Mutex (per GDALRasterBand)
-  RBM Mutex (per GDALRasterBlockManager)

In order to prevent deadlocks, a priority of the mutexes is established
in the order they are listed. For example if you have the Band Mutex,
you may not obtain the Dataset RW Mutex, unless it was obtained prior to
the Band Mutex being obtained. However, the goal should always be to
never have more then mutex at a time!

Dataset RW Mutex
''''''''''''''''

The objective of the Dataset RW Mutex is to protect the data stored
within the the GDALRasterBlocks associated with a dataset, and lock
during large Read or Write operations. This prevents two different
threads from using memcpy on the same GDALRasterBlock at the same time.
This mutex normally lies within the GDALDataset, but in the case of a
standalone GDALRasterBand, it utilizes a new mutex on the Band.

Band Mutex
''''''''''

The objective of the Band Mutex is to manage the control of the array of
blocks in the GDALRasterBand, and manages the locking of the
GDALRasterBlocks. This is a per GDALRasterBand Mutex.

RBM Mutex
'''''''''

The objective of the RBM Mutex is to manage control of the LRU cache.
This mutex is responsible for the control of the management of the
cache's linked list and total amount of data stored in the cache.

Pros
^^^^

This is a much more simple solution of the two different possible
solutions. Since the protection of the Blocks are done at the Dataset
level, it prevents the problem of some drivers such as geotiff where
more then one band might be accessed in the reading or writing of one
band. Therefore with out this protection here it could cause issues if
locking was just at a band level per block's data.

Cons
^^^^

This solution is not perhaps the most optimal way to lock because the
protection of the IReadBlock, IWriteBlock, and IRasterIO routines is
over the entire dataset. This is very limiting when you are reading the
same dataset in a threaded environment, because it is not possible to
read more then one block at a time.

.. _solution-2-rw-mutex-in-gdalrasterblock-:

Solution 2 (RW Mutex in GDALRasterBlock )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _mutexes-1:

Mutexes
^^^^^^^

For solution 2 the three mutexes are:

-  Band Mutex (per GDALRasterBand)

   -  RBM Mutex (per GDALRasterBlockManager)
   -  Block RW Mutex (per GDALRasterBlock)

In order to prevent deadlocks the band mutex has priority. This means
that you can not get the Band Mutex if you have the RBM or Blow RW
Mutex, unless you already had the Band Mutex prior to this. You may not
obtain the Block mutex and the RBM mutex at the same time.

.. _band-mutex-1:

Band Mutex
''''''''''

The objective of the Band Mutex is to manage the control of the array of
blocks in the GDALRasterBand, and manages the locking of the
GDALRasterBlocks. This is a per GDALRasterBand Mutex.

.. _rbm-mutex-1:

RBM Mutex
'''''''''

The objective of the RBM Mutex is to manage control of the LRU cache.
This mutex is responsible for the control of the management of the
cache's linked list and total amount of data stored in the cache.

Block RW Mutex
''''''''''''''

The objective of the Block RW Mutex is to protect the data stored within
the the GDALRasterBlocks associated with a dataset, and lock during
large Read or Write operations. This prevents two different threads from
using memcpy on the same GDALRasterBlock at the same time. It is created
on a per block basis.

.. _pros-1:

Pros
^^^^

This is probably the most complete solution to making an intensive and
fast threaded solution for the blocking. This is because the IReadWrite,
IWriteBlock, and IRasterIO now are able to possibly pass a mutex with
their calls, as a void pointer pointer. A change was made to the mutex
as well such that a void pointer pointer that is NULL passed to
CPLMutexHolderD, will not result in any pointer being created or any
locking to occur. This means much of the behavior of the existing code
can be maintained by simply passing a NULL value for the mutex. All of
these changes allow the drivers to maintain much more control over the
way that locking occurs when protecting the data inside a block.

.. _cons-1:

Cons
^^^^

Obviously, this is a much more complex solution and therefore is harder
to manage. It means that writing a driver is not as trivial as before
and care must be taken in how locking is done within the driver in order
to prevent deadlocks and maintain thread safety. The other issue that
might arise from this is a slight slow down in non-threaded code because
of the extra cycles spent locking data that will not be accessed in a
threaded manner. Additionally, it might have issues in windows if too
many mutexes are created (as there are quite a few more since it is a
per GDALRasterBlock mutex). (Note: Not sure how I will be able to test
this properly?)
