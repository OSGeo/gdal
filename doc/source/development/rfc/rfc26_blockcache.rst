.. _rfc-26:

================================================================================
RFC 26: GDAL Block Cache Improvements
================================================================================

Authors: Tamas Szekeres, Even Rouault

Contact: szekerest@gmail.com, even.rouault at spatialys.com

Status: Adopted, implemented

Implementation version: GDAL 2.1

Summary and rationale
---------------------

GDAL maintains an in-memory cache for the raster blocks fetched from the
drivers and ensures that the second attempt to access the same block
will be served from the cache instead of the driver. This cache is
maintained in a per-band fashion and an array is allocated for the
pointers for each blocks (or sub-blocks). This approach is not
sufficient with large raster dimensions (or large virtual rasters ie.
with the WMS/TMS driver), which may cause out of memory errors in
GDALRasterBand::InitBlockInfo, as raised in #3224

For example, a band of a dataset at level 21 with a GoogleMaps tiling
requires 2097152x2097152 tiles of 256x256 pixels. This means that GDAL
will try to allocate an array of 32768x32768 = 1 billion elements (32768
= 2097152 / 64). The size of this array is 4 GB on a 32-bit build, so it
cannot be allocated at all. And it is 8 GB on a 64-bit build (even if
this is generally only virtual memory reservation but not actually
allocation of physical pages of memory, due to over-commit mechanism of
the operating system). At dataset closing, this means that those 1
billion cells will have to be explored to discover remaining cached
blocks. In reality, all above figures must be multiplied by 3 for a RGB
(or 4 for a RGBA) dataset.

In the hash set implementation, memory allocation depends directly on
the number of cached blocks. Typically with the default GDAL_CACHEMAX
size of 40 MB, only 640 blocks of 256x256 pixels can be simultaneously
cached (for all datasets).

Main concepts
-------------

Awareness of thread-safety issues is crucial in the design of block
caching. In gdalrasterblock.cpp, a static linked list is maintained so
as to track the access order of the blocks and keep the size of the
cache within a desired limit by dropping the oldest blocks out of the
list. This linked list is shared among all the datasets and bands in
GDAL (protected by a hRBMutex) and a thread on each band, when reading a
new block, may also trigger a GDALRasterBand::UnreferenceBlock call on
another band within the scope of this mutex. GDALRasterBand::FlushBlock
will also access the data structure of the band level cache by removing
the corresponding tile from the array or the hashtable.

In GDAL 2.0, some issues related to thread-safety (#3225, #3226) have
been fixed and this RFC still preserves those scenarios as safe.

The changes of this RFC consist in moving away from the GDALRasterBand
class the logic to access to a cached block, to add or remove it. This
is done with the new GDALAbstractBandBlockCache class. The current array
based logic is moved into the new GDALArrayBandBlockCache class, and the
new hashset based logic in GDALHashsetBandBlockCache.

For the array based implementation, due to the "static" nature of the
hosting structure (an array), no special care is needed when reading or
writing a cell from concurrent threads. The only special care that must
be taken is to prevent a given cell (block) to be accessed concurrently.
For example we want to avoid TryGetLockedBlockRef() to return a block
that is being freed by another thread from
GDALRasterBlock::FlushCacheBlock() or Internalize(). For that, the
nRefCount member of GDALRasterBlock is now accessed and modified only
through atomic functions to increase, decrease or compare-and-swap its
value.

For the hash set based implementation, the base implementation of hash
set data structure done in in cpl_hash_set.h / cpl_hash_set.cpp is not
thread safe by default. So GDALHashsetBandBlockCache has a dedicated
mutex to protect all reads, additions and removals from the hash set. No
dead-lock with the hRBMutex can occurs since no operations done under
the hashset mutex involves calling any method from GDALRasterBlock.

We could potentially have reused the hRBMutex to protect the hash set,
but this would have increased the contention of the hRBMutex
unnecessarily.

By default, the selection between the array based and the hashtable
based approaches is based on the following rule: if the dataset has more
than 1 million blocks, the hashset based implementation is used,
otherwise the array based implementation is used. The new
GDAL_OF_ARRAY_BLOCK_ACCESS and GDAL_OF_HASHSET_BLOCK_ACCESS open flags
can also be passed to GDALOpenEx() to override this choice. The
GDAL_BAND_BLOCK_CACHE configuration option can also be set to ARRAY or
HASHSET.

The hashset based implementation could potentially be the default
implementation in all cases (performance comparisons done with the
autotest/cpp/testblockcache utility with 4 or 8 cores show non
measurable differences), but in theory the array based implementation
offers less contention of the hRBMutex, so should be more scalable when
using lots of cores. And as work has been done during GDAL 2.0 to
improve the scalability, it might be prudent for now to remain on the
array based implementation on rasters of modest size.

Not completely linked with this RFC, a few changes have been done to
limit the number of allocation/deallocation of objects (GDALRasterBlock
instances, as well as an internal element of CPLHashSet), which has an
effect on scalability since memory allocation routines involve
synchronization between threads.

Implementation
--------------

To implement the addition the following changes is made in the GDAL
codebase:

-  port/cpl_hash_set.cpp / port/cpl_hash_set.h: CPLHashSetClear()
   function added to remove all the elements in one operation.

-  port/cpl_hash_set.cpp / port/cpl_hash_set.h:
   CPLHashSetRemoveDeferRehash() function added to remove one element
   quickly. That is to say the potential resizing of the array used
   internally is deferred to a later operation

-  port/cpl_hash_set.cpp / port/cpl_hash_set.h: improvements to
   "recycle" links from the linked lists and avoid useless
   malloc()/free().

-  port/cpl_atomic_ops.cpp: addition of CPLAtomicCompareAndExchange()

-  gcore/gdal.h: additions of GDAL_OF_DEFAULT_BLOCK_ACCESS,
   GDAL_OF_ARRAY_BLOCK_ACCESS and GDAL_OF_HASHSET_BLOCK_ACCESS values.

-  gcore/gdal_priv.h: definition of GDALAbstractBandBlockCache class,
   and GDALArrayBandBlockCacheCreate() and
   GDALHashSetBandBlockCacheCreate() functions. Modifications of
   GDALRasterBand, GDALDataset and GDALRasterBlock definitions.

-  gcore/gdalrasterband.cpp: InitBlockInfo() instantiates the
   appropriate band block cache implementation.

-  gcore/gdalrasterband.cpp: the AdoptBlock(), UnreferenceBlock(),
   FlushBlock() and TryGetLockedBlockRef() methods delegate to the
   actual band block cache implementation.

-  gcore/gdalrasterband.cpp: AddBlockToFreeList() is added and delegate
   to GDALAbstractBandBlockCache

-  gcore/gdalrasterblock.cpp: SafeLockBlock() is replaced by TakeLock()

-  gcore/gdalrasterblock.cpp: RecycleFor() method added to recycle an
   existing block object to save a few new/delete calls (used by
   GDALAbstractBandBlockCache::CreateBlock())

-  gcore/gdalrasterblock.cpp: Internalize() or FlushCacheBlock() no
   longer directly free a block (they still free or recycle its pData
   member), but provide it to GDALRasterBand::AddBlockToFreeList() for
   layer reuse.

-  gcore/gdalrasterblock.cpp: DropLockForRemovalFromStorage() is added
   to avoid racing destruction of blocks between
   GDALRasterBand::FlushCache() or FlushBlock() with
   GDALRasterBlock::Internalize() or FlushCacheBlock().

-  gcore/gdalabstractbandblockcache.cpp: added. Contains logic to keep
   instantiated GDALRasterBlock that were discarded by the global block
   manager for their later reuse. Saves a few new/delete calls.

-  gcore/gdalarraybandblockcache.cpp: the GDALArrayBandBlockCache class
   implementation with mostly the existing code

-  gcore/gdalhashsetbandblockcache.cpp: the new
   GDALHashsetBandBlockCache class implementation

Backward Compatibility
----------------------

This implementation retains the backward compatibility with the existing
API. The C++ ABI of GDALRasterBand, GDALDataset and GDALRasterBlock is
modified.

Performance impacts
-------------------

The array based implementation after this RFC should still show the same
performance than the current implementation (potentially very slightly
improved with the recycling of blocks). Confirmed by tests with
autotest/cpp/testblockcache.

Documentation
-------------

This change doesn't affect the existing user documentation.

Testing
-------

The autotest/cpp/testblockcache utility is now run by the "quick_test"
target of autotest/cpp/Makefile with GDAL_BAND_BLOCK_CACHE=HASHSET in
additions to the array based implementation.

A new autotest/cpp/testblockcachelimits utility has been developed to
test a few racing situations. As races are hard to trigger, the code of
GDALRasterBlock has been instrumented to allow sleeping in particular
places, enabling races to be reliably simulated.

Implementation
--------------

Tamas Szekeres had provided an initial version of this RFC. It has been
restructured and ported on GDAL 2.0 by Even Rouault (sponsored by `LINZ
(Land Information New Zealand) <http://www.linz.govt.nz/>`__)

References
----------

The proposed implementation lies in the "rfc26_bandblockcache" branch of
the
`https://github.com/rouault/gdal2/tree/rfc26_bandblockcache <https://github.com/rouault/gdal2/tree/rfc26_bandblockcache>`__
repository.

The list of changes:
`https://github.com/rouault/gdal2/compare/rfc26_bandblockcache <https://github.com/rouault/gdal2/compare/rfc26_bandblockcache>`__

Related bugs: #3264, #3224.

Voting History
--------------

+1 from EvenR, DanielM, TamasS. +0 from JukkaR
