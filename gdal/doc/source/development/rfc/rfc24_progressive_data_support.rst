.. _rfc-24:

================================================================================
RFC 24: GDAL Progressive Data Support
================================================================================

Author: Norman Barker, Frank Warmerdam

Contact: nbarker@ittvis.com, warmerdam@pobox.com

Status: Adopted

Summary
-------

Provide an interface for asynchronous/streaming data access in GDAL. The
initial implementation is for JPIP, but should be generic enough to
apply to other streaming / progressive approaches. Background on the
JPIP (Kakadu) implementation can be found in [wiki:rfc24_jpipkak].

Interfaces
----------

GDALAsyncReader
~~~~~~~~~~~~~~~

This new class is intended to represent an active asynchronous raster
imagery request. The request includes information on a source window on
the dataset, a target buffer size (implies level of decimation or
replication), the buffer type, buffer interleaving, data buffer and
bands being requested. Essentially the same sort of information that is
passed in a GDALDataset::!RasterIO() request.

The GetNextUpdatedRegion() method can be used to wait for an update to
the imagery buffer, and to find out what area was updated. The
LockBuffer() and UnlockBuffer() methods can be used to temporarily
disable updates to the buffer while application code accesses the
buffer.

While an implementation of the simple accessors is provided as part of
the class, it is intended that the class be subclassed as part of
implementation of a particular driver, and custom implementations of
GetNextUpdatedRegion(), LockBuffer() and UnlockBuffer() provided.

{{{ class CPL_DLL GDALAsyncReader { protected: GDALDataset\* poDS; int
nXOff; int nYOff; int nXSize; int nYSize; void \* pBuf; int nBufXSize;
int nBufYSize; GDALDataType eBufType; int nBandCount; int\* panBandMap;
int nPixelSpace; int nLineSpace; int nBandSpace; long nDataRead;

public: GDALAsyncReader(GDALDataset\* poDS = NULL); virtual
~GDALAsyncReader();

::

   GDALDataset* GetGDALDataset() {return poDS;}
   int GetXOffset() {return nXOff;}
   int GetYOffset() {return nYOff;}
   int GetXSize() {return nXSize;}
   int GetYSize() {return nYSize;}
   void * GetBuffer() {return pBuf;}
   int GetBufferXSize() {return nBufXSize;}
   int GetBufferYSize() {return nBufYSize;}
   GDALDataType GetBufferType() {return eBufType;}
   int GetBandCount() {return nBandCount;}
   int* GetBandMap() {return panBandMap;}
   int GetPixelSpace() {return nPixelSpace;}
   int GetLineSpace() {return nLineSpace;}
   int GetBandSpace() {return nBandSpace;}

   virtual GDALAsyncStatusType GetNextUpdatedRegion(double dfTimeout,
                                                    int* pnBufXOff,
                                                    int* pnBufYOff,
                                                    int* pnBufXSize,
                                                    int* pnBufXSize) = 0;

   virtual int LockBuffer( double dfTimeout );
   virtual void UnlockBuffer(); 

   friend class GDALDataset;

}; }}}

GetNextUpdatedRegion()
~~~~~~~~~~~~~~~~~~~~~~

::

   GDALAsyncStatusType 
   GDALAsyncRasterio::GetNextUpdatedRegion(int dfTimeout,
                                           int* pnBufXOff, int* pnBufYOff,
                                           int* pnBufXSize, int* pnBufXSize);

   int dfTimeout;
     The amount of time to wait for results measured  in seconds.  If this is
     zero available work may be processed but no waiting for the arrival of more
     imagery should be done.  A value of -1.0 means wait an infinite amount of
     time for new data.  Processing available imagery may still take an 
     arbitrary amount of time.

   int *pnBufXOff, *pnBufYOff, *pnBufXSize, *pnBufYSize;
     The window of data updated within the async io imagery buffer is returned in
     these variables. This information can be used to limit screen redraws or other
     processing to the portion of the imagery that may have changed.

The async return status list is as follows, and will be declared in
gdal.h.

::

   typedef enum 
   {   
       GARIO_PENDING = 0,
       GARIO_UPDATE = 1,
       GARIO_ERROR = 2,
       GARIO_COMPLETE = 3,
       GARIO_TypeCount = 4
   } GDALAsyncStatusType;

The meaning as a return value is:

-  GARIO_PENDING: No imagery was altered in the buffer, but there is
   still activity pending, and the application should continue to call
   GetNextUpdatedRegion() as time permits.
-  GARIO_UPDATE: Some of the imagery has been updated, but there is
   still activity pending.
-  GARIO_ERROR: Something has gone wrong. The asynchronous request
   should be ended.
-  GARIO_COMPLETE: An update has occurred and there is no more pending
   work on this request. The request should be ended and the buffer
   used.

GDALDataset
~~~~~~~~~~~

The GDALDataset class is extended with methods to create an asynchronous
reader, and to cleanup the asynchronous reader. It is intended that
these methods would be subclassed by drivers implementing asynchronous
data access.

::

       virtual GDALAsyncReader* 
           BeginAsyncReader(int nXOff, int nYOff, int nXSize, int nYSize,
                              void *pBuf, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int* panBandMap,
                              int nPixelSpace, int nLineSpace, int nBandSpace,
                              char **papszOptions);
       virtual void EndAsyncReader(GDALAsyncReader *);

It is expected that as part of gdal/gcore a default !GDALAsyncReader
implementation will be provided that just uses GDALDataset::!RasterIO()
to perform the request as a single blocking request. However, this
default implementation will ensure that applications can use the
asynchronous interface without worrying whether a particular format will
actually operate asynchronously.

GDALDriver
~~~~~~~~~~

In order to provide a hint to applications whether particular formats
support asynchronous IO, we will add a new metadata item on the
GDALDriver of implementing formats. The metadata item will be
"DCAP_ASYNCIO" (macro GDAL_DCAP_ASYNCIO) and will have the value "YES"
if asynchronous IO is available.

Implementing drivers will do something like this in their driver setup
code:

::

      poDriver->SetMetadataItem( GDAL_DCAP_ASYNCIO, "YES" );

GDALRasterBand
~~~~~~~~~~~~~~

There are no changes to the GDALRasterBand interface for asynchronous
raster IO. Asynchronous IO requests can only be made at the dataset
level, not the band.

C API
-----

The following C API wrappers for the C++ classes and methods will be
added. Note that at this time there is no intention to provide C
wrappers for all the GDALAsyncReader accessors since the provided
information is already available in the application from the call
launching the async io.

::

   typedef void *GDALAsyncReaderH;

   GDALAsyncStatusType CPL_DLL CPL_STDCALL 
   GDALGetNextUpdatedRegion(GDALAsyncReaderH hARIO, double dfTimeout,
                            int* pnXBufOff, int* pnYBufOff, 
                            int* pnXBufSize, int* pnYBufSize );
   int CPL_DLL CPL_STDCALL GDALLockBuffer(GDALAsyncReaderH hARIO,double dfTimeout);
   void CPL_DLL CPL_STDCALL GDALUnlockBuffer(GDALAsyncReaderH hARIO); 

   GDALAsyncReaderH CPL_DLL CPL_STDCALL 
   GDALBeginAsyncReader(GDALDatasetH hDS, int nXOff, int nYOff,
                          int nXSize, int nYSize,
                          void *pBuf, int nBufXSize, int nBufYSize,
                          GDALDataType eBufType,
                          int nBandCount, int* panBandMap,
                          int nPixelSpace, int nLineSpace, int nBandSpace,
                          char **papszOptions);
   void  CPL_DLL CPL_STDCALL 
   GDALEndAsyncReader(GDALDatasetH hDS, GDALAsyncReaderH hAsynchRasterIOH);

SWIG
----

It is intended that all the above functions in the C API will be wrapped
for SWIG.

Driver Implementations
----------------------

A full implementation of the Asynchronous API will be provided as the
JPIPKAK driver - a JPIP protocol implementation using the Kakadu
library.

At this time, no other implementations are planned.

Testing
-------

Some testing of the asynchronous api against normal drivers will be
added in the test suite, as well as testing of the JPIPKAK driver in
asynchronous and conventional data access methods.

Also, a new commandline program, gdalasyncread, is implemented which
provides a mechanism to test the async API from the commandline. It
takes a subset of the gdal_translate commandline options.

::

   Usage: gdalasyncread [--help-general]
          [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/
                CInt16/CInt32/CFloat32/CFloat64}]
          [-of format] [-b band]
          [-outsize xsize[%] ysize[%]]
          [-srcwin xoff yoff xsize ysize]
          [-co "NAME=VALUE"]* [-ao "NAME=VALUE"]
          src_dataset dst_dataset

