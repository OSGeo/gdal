.. _rfc-91:

=============================================================
RFC 91: GDALDataset::Close() method
=============================================================

============== =============================================
Author:        Even Rouault
Contact:       even.rouault at spatialys.com
Started:       2023-Jan-20
Status:        Adopted, implemented
Target:        GDAL 3.7
============== =============================================

Summary
-------

This RFC aims at providing more robust error detection during dataset closing,
in particular for datasets in creation/update mode.

It modifies the GDALDataset::FlushCache() method and GDALFlushCache()
function to return an error code, adds a GDALDataset::Close() virtual method and
modifies GDALClose() to return an error code.

Motivation
----------

A lot of drivers that have write capabilities may do write operations in their
destructors, or just close the file descriptor owned by the dataset. Currently
there is no clean way of catching errors that might occur. This recently bit
`Fiona <https://github.com/Toblerity/Fiona/issues/1169>` where the GeoJSON driver
may emit an error in its dataset destructor when writing the content of a file
to a cloud object storage.

Code that currently wants to detect if GDALClose() causes an error has to
do something like the following:

.. code-block:: c++

    CPLErr eErrCodeBefore = CPLGetLastErrorType();
    GDALClose(hOutDS);
    if( eErrCodeBefore == CE_None && CPLGetLastErrorType() != CE_None )
        printf("Error occurred during GDALClose()!\n")

This is awkward and there is a risk that something in the chain of calls would
clean the error state. It is thus desirable for the GDALClose() method to
return an error code.

Technical details
-----------------

The GDALDataset::FlushCache() method is modified to return a CPLErr return value.

A new virtual method GDALDataset::Close() is added.

.. code-block:: c++

    /** Do final cleanup before a dataset is destroyed.
     *
     * This method is typically called by GDALClose() or the destructor of a
     * GDALDataset subclass. It might also be called by C++ users before
     * destroying a dataset. It should not be called on a shared dataset whose
     * reference count is greater than one.
     *
     * It gives a last chance to the closing processus to return an error code if
     * something goes wrong, in particular in creation / update scenarios where
     * file write or network communication might occur when finalizing the dataset.
     *
     * Implementations should be robust to this method to be called several times
     * (on subsequent calls, it should do nothing and return CE_None).
     * Once it has been called, no other method than Close() or the dataset
     * destructor should be called. RasterBand or OGRLayer owned by the dataset
     * should be assumed as no longer being valid.
     *
     * If a driver implements this method, it must also call it from its
     * dataset destructor.
     *
     * @since GDAL 3.7
     */

A typical implementation might look as the following:

.. code-block:: c++

    MyDataset::~MyDataset()
    {
       try
       {
           MyDataset::Close();
       }
       catch (const std::exception &exc)
       {
           // If Close() can throw exception
           CPLError(CE_Failure, CPLE_AppDefined,
                    "Exception thrown in MyDataset::Close(): %s",
                    exc.what());
       }
       catch (...)
       {
           // If Close() can throw exception
           CPLError(CE_Failure, CPLE_AppDefined,
                    "Exception thrown in MyDataset::Close()");
       }
    }

    CPLErr MyDataset::Close()
    {
        CPLErr eErr = CE_None;
        if( nOpenFlags != OPEN_FLAGS_CLOSED )
        {
            if( MyDataset::FlushCache(true) != CE_None )
                eErr = CE_Failure;

            // Do something driver specific
            if (m_fpImage != nullptr)
            {
                if( VSIFCloseL(m_fpImage) != 0 )
                {
                    CPLError(CE_Failure, CPLE_FileIO, "VSIFCloseL() failed");
                    eErr = CE_Failure;
                }
            }

            // Call parent Close() implementation.
            if( MyParentDatasetClass::Close() != CE_None )
                eErr = CE_Failure;
        }
        return eErr;
    }

The default GDALDataset::Close() implementation sets nOpenFlags to OPEN_FLAGS_CLOSED


C API
-----

GDALClose() and GDALFlushCache() are modified to return a CPLErr return value.

Backward compatibility
----------------------

This is an ABI change that should not require more than rebuilding applications
against GDAL headers.

For out-of-tree drivers that implement GDALDataset::FlushCache(), they need to
take into account the change in its signature.

Out-of-tree drivers that have write capabilities are also encouraged to
implement GDALDataset::Close().

Limitations
-----------

Not all drivers will be modified to implement Close() in the candidate
implementation, and even those modified might call internal methods of the
driver that do not do error propagation. Consequently, further work might be
needed on a case-by-case to improve driver implementations.

SWIG Bindings
-------------

The destructor of gdal.Dataset is modified to test the return value of GDALClose()
and emits a CPLError(CE_Failure, ...) if the error state is clean (normally
it should not)

C/C++ command line utilities
----------------------------

C/C++ command line utilities are modified to test the return value of GDALClose()
on output datasets, and return a non-zero return code for the process if GDALClose()
returns an error.

Testing
-------

autotest/pymod/gdaltest.py::testCreate() is modified to call FlushCache() and
test its return value.

The existing tests of the C/C++ command line utilities test GDALClose() modified
behavior.

Issues / pull requests
----------------------

Addresses https://github.com/OSGeo/gdal/issues/6886

https://github.com/OSGeo/gdal/compare/master...rouault:gdal:dataset_FlushCache_return_CPLErr?expand=1
contains a candidate implementation.

The candidate implementation does the following:

* Update all drivers that implements GDALDataset::FlushCache().
* Implements GDALDataset::Close() for all drivers that derive from RawDataset.
* Implements GDALDataset::Close() in the GeoJSON, GTiff, ODS, XLSX, SQLite,
  GPKG, netCDF, JP2OpenJPEG, FlatGeoBuf and OpenFileGDB drivers.
* Modifies all C/C++ command line utilities to test the return value of GDALClose()
* Tests GDALClose() return value in gdal.Dataset destructor.

Voting history
--------------

+1 from PSC members KurtS, MateuszL and me, and +0 from JukkaR
