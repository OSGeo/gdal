.. _rfc-11:

================================================================================
RFC 11: Fast Format Identification
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted (and Implemented)

Summary
-------

This RFC aims to add the ability for applications to quickly identify
what files in the file system are GDAL supported file formats without
necessarily opening any of them. It is mainly intended to allow GUI file
browsers based on file types.

This is accomplished by extending the GDALOpenInfo structure to hold
more directory context, and by adding an Identify() method on the
GDALDriver which a driver can implement to quickly identify that a file
is of a given format without doing a more expensive Open() operation.

GDALOpenInfo
------------

The Open() (or Identify()) methods of many drivers need to probe for
files associated with the target file in order to open or identify a
file as being of a particular format. For instance, in order to open an
ESRI BIL file (EHDR driver) it is necessary to probe for a driver with
the same basename as the target file, but the extension .hdr. Currently
this is typically accomplished with VSIFStatL() calls or similar which
can be fairly expensive.

In order to reduce the need for such searches touch the operating system
file system machinery, the GDALOpenInfo structure will be extended to
hold an optional list of files. This is the list of all files at the
same level in the file system as the target file, including the target
file. The filenames will *not* include any path components, are an
essentially just the output of CPLReadDir() on the parent directory. If
the target object does not have filesystem semantics then the file list
should be NULL.

The following is added to GDALOpenInfo:

::

              GDALOpenInfo( const char * pszFile, GDALAccess eAccessIn, char **papszSiblings );
       char **papszSiblingFiles;

The new constructor allows the file list to be passed in to populate the
papszSiblingFiles member (the argument will be copied). The existing
default constructor will use CPLGetDirname() to get the directory of the
passed pszFile, and CPLReadDir() to read the corresponding file list.
The new constructor is primarily aimed at efficient implementation of
the later GDALIdentifyDriver() function, avoiding re-reading the file
list for each file to be tested.

Identify()
----------

The GDALDriver class will be extended with the following function:

::

     int      (*pfnIdentify)( GDALOpenInfo * );

When implemented by a driver, the function is intended to return TRUE
(non-zero) if the driver determines that the file passed in via
GDALOpenInfo appears to be of the format the driver is implemented for.
To call this applications should call the new function:

::

     GDALDriverH *GDALIdentifyDriver( const char *pszDatasource, const char **papszDirFiles );

Internally GDALIdentifyDriver() will do the following

1. A GDALOpenInfo structure will be initialized based on pszDatasource
   and papszDirFiles.
2. It will iterate over all drivers similarly to GDALOpen(). For each
   driver it will use the pfnIdentify function if available, otherwise
   it will use the pfnOpen() method to establish if the driver supports
   the file.
3. It will return the driver handle for the first driver to respond
   positively or NULL if none accept it.

Driver Changes
--------------

In theory it is not necessary for any drivers to be modified, since
GDALIdentifyDriver() will fallback to using the pfnOpen function to
test. But in practice, no optimization is achieved unless at least some
drivers (hopefully those for which Open can be very expensive) are
updated. Part of the ongoing effort then is to implement identify
functions for GDAL drivers.

Generally speaking it should be easy to craft an identify function from
the initial test logic in the open function. For instance, the GeoTIFF
driver might be changed like this:

::

   int GTiffDataset::Identify( GDALOpenInfo * poOpenInfo )

   {
   /* -------------------------------------------------------------------- */
   /*      We have a special hook for handling opening a specific          */
   /*      directory of a TIFF file.                                       */
   /* -------------------------------------------------------------------- */
       if( EQUALN(poOpenInfo->pszFilename,"GTIFF_DIR:",10) )
           return TRUE;

   /* -------------------------------------------------------------------- */
   /*  First we check to see if the file has the expected header   */
   /*  bytes.                              */    
   /* -------------------------------------------------------------------- */
       if( poOpenInfo->nHeaderBytes < 2 )
           return FALSE;

       if( (poOpenInfo->pabyHeader[0] != 'I' || poOpenInfo->pabyHeader[1] != 'I')
           && (poOpenInfo->pabyHeader[0] != 'M' || poOpenInfo->pabyHeader[1] != 'M'))
           return FALSE;

       // We can't support BigTIFF files for now. 
       if( poOpenInfo->pabyHeader[2] == 43 && poOpenInfo->pabyHeader[3] == 0 )
           return FALSE;
    

       if( (poOpenInfo->pabyHeader[2] != 0x2A || poOpenInfo->pabyHeader[3] != 0)
           && (poOpenInfo->pabyHeader[3] != 0x2A || poOpenInfo->pabyHeader[2] != 0) )
           return FALSE;

       return TRUE;
   }

The open might then be modified to use the identify function to avoid
duplicating the test logic.

::

   GDALDataset *GTiffDataset::Open( GDALOpenInfo * poOpenInfo )

   {
       TIFF    *hTIFF;

       if( !Identify( poOpenInfo ) )
           return NULL;

   /* -------------------------------------------------------------------- */
   /*      We have a special hook for handling opening a specific          */
   /*      directory of a TIFF file.                                       */
   /* -------------------------------------------------------------------- */
       if( EQUALN(poOpenInfo->pszFilename,"GTIFF_DIR:",10) )
           return OpenDir( poOpenInfo->pszFilename );

       GTiffOneTimeInit();
   ...

Drivers which require header files such as the EHdr driver might
implement Identify() like this:

::

   int EHdrDataset::Identify( GDALOpenInfo * poOpenInfo )

   {
       int     i, bSelectedHDR;
       const char  *pszHDRFilename;
       
   /* -------------------------------------------------------------------- */
   /*  We assume the user is pointing to the binary (ie. .bil) file.   */
   /* -------------------------------------------------------------------- */
       if( poOpenInfo->nHeaderBytes < 2 )
           return FALSE;

   /* -------------------------------------------------------------------- */
   /*      Now we need to tear apart the filename to form a .HDR           */
   /*      filename.                                                       */
   /* -------------------------------------------------------------------- */
       CPLString osBasename = CPLGetBasename( poOpenInfo->pszFilename );
       pszHDRFilename = CPLFormCIFilename( "", osBasename, "hdr" );

       if( CSLFindString( poOpenInfo->papszSiblingFiles, pszHDRFilename) )
           return TRUE;
       else
           return FALSE;
   }

During the initial implementation a variety of drivers will be updated,
including the following. As well some performance and file system
activity logging will be done to identify drivers that are currently
expensive.

-  HFA
-  GTiff
-  JPEG
-  PNG
-  GIF
-  HDF4
-  DTED
-  USGS DEM
-  MrSID
-  JP2KAK
-  ECW
-  EHdr
-  RST

CPLReadDir()
------------

Currently the VSIMemFilesystemHandler implemented in cpl_vsi_mem.cpp
which provides "filesystem like" access to objects in memory does not
implement directory reading services. In order to properly populate the
directory listing this will need to be added.

To do this the CPLReadDir() function will also need to be reimplemented
to use VSIFilesystemHandler::ReadDir() instead of direct implementation
in cpl_dir.cpp. The win32 and unix/posix implementations of
VSIFilesystemHandler::ReadDir() already exist. This should essentially
complete the virtualization of filesystem access services.

CPLReadDir() will also be renamed VSIReadDir() but with a stub under the
old name available for backward compatibility.

Compatibility
-------------

There are no anticipated backward compatibility problems. However
forward compatibility will be affected, in that drivers updated in trunk
with the Identify function will not be able to be ported back into 1.4
builds and used their. Unmodified drivers, and externally maintained
drivers should not be impacted by this development.

SWIG Implications
-----------------

The GDALIdentifyDriver() and VSIReadDir() functions will need to be
exposed via SWIG.

Regression Testing
------------------

A test script for the Identify() function will be added to the
autotest/gcore directory. It will include testing of identify in a
/vsimem memory collection.

Implementation Plan
-------------------

The new features will be implemented by Frank Warmerdam in *trunk* for
the GDAL/OGR 1.5.0 release.

Performance Tests
-----------------

A very quick test introducing the Identify without actually opening
changed the time to identify all files in a directory with 70 TIFF files
(on an NFS share) from 2 seconds to 0.5 seconds. So saving the overhead
of actually opening files can be significant for some formats, including
very common ones like GeoTIFF.
