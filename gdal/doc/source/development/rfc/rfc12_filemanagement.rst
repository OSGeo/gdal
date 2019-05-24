.. _rfc-12:

================================================================================
RFC 12: Improved File Management
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted / Implemented

Summary
-------

Some applications using GDAL have a requirement to provide file
management operations through the GUI. This includes deleting, renaming,
moving and packaging up datasets which often requires operations on
several associated files. This RFC introduces an operation on a
GDALDataset to identify all the dataset files, and operations to move or
copy them.

GetFileList()
-------------

The following new virtual method is added on the GDALDataset class, with
an analygous C function.

::

      virtual char   **GDALDataset::GetFileList(void);

The method is intended to return a list of files associated with this
open dataset. The return is a NULL terminated string list which becomes
owned by the caller and should be deallocated with CSLDestroy().

The default implementation tests the name of the datasource to see if it
is a file, and if so it is returned otherwise an empty list is returned.
If the default overview manager is active, and has overviews, those will
also be included in the file list. The default implementation also
checks for world files, but only those with extensions based on the
original files extension (ie. .tfw or .tifw for .tif) but does not
search for .wld since that is not very specific.

The GDALPamDataset::GetFileList() method will add the ability to find
.aux and .aux.xml files associated with a dataset to the core default
behavior.

pfnRename()
-----------

The following new function is added to the GDALDriver class.

::

       CPLErr       (*pfnRename)( const char *pszNewName, const char *pszOldName );

Also a corresponding function is added to the C API.

::

       CPLErr        GDALRenameDataset( GDALDriverH hDriver, const char *pszNewName, const char *pszOldName );

Note that renaming is done by the driver, but the dataset to be operated
on should *not* be open at the time. GDALRenameDataset() will invoke
pfnRename if it is non-NULL.

If pfnRename is NULL the default implementation will be used which will
open the dataset, fetch the file list, close the dataset, and then try
to rename all the files (based on shared basenames). The default rename
operation will fail if it is unable to establish a relationship between
the files (ie. a common basename or stem) to indicate how the group of
files should be rename to the new pattern.

Optionally a NULL hDriver argument may be passed in, in which case the
appropriate driver will be selected by first opening the datasource.

CPLMoveFile()
-------------

The POSIX rename() function on which VSIRename() is usually based does
not normally allow renaming files between file systems or between
different kinds of file systems (ie. /vsimem to C:/abc). In order to
implement GDALRenameDataset() such that it works efficiently within a
file system, but still works between file systems, a new operation will
be added to gdal/port. This is the CPLMoveFile() function which will
first try a VSIRename(). If that fails it will use CPLCopyFile() to copy
the whole file and then VSIUnlink() to get rid of the old file.

::

     int CPLMoveFile( const char *pszNewFilename, const char *pszOldFilename );

The return value will be zero on success, otherwise an errno style
value.

It should be noted that in some error conditions, such as the
destination file system running out of space during a copy, it may
happen that some files for a dataset get renamed, and some do not
leaving things in an inconsistent state.

pfnCopyFiles()
--------------

The following new function is added to the GDALDriver class.

::

       CPLErr       (*pfnCopyFiles)( const char *pszNewName, const char *pszOldName );

Also a corresponding function is added to the C API.

::

       CPLErr        GDALCopyDatasetFiles( GDALDriverH hDriver, const char *pszNewName, const char *pszOldName );

Note that copying is done by the driver. The dataset may be opened, but
if opened in update mode it may be prudent to first do a flush to
synchronize the in-process state with what is on disk.
GDALCopyDatasetFiles() will invoke pfnCopyFiles if it is non-NULL.

If pfnCopy is NULL the default implementation will be used which will
open the dataset, fetch the file list, close the dataset, and then try
to copy all the files (based on shared basenames). The default copy
operation will fail if it is unable to establish a relationship between
the files (ie. a common basename or stem) to indicate how the group of
files should be renamed to the new pattern.

Optionally a NULL hDriver argument may be passed in, in which case the
appropriate driver will be selected by first opening the datasource.

Copy is essentially the same as Rename, but the original files are
unaltered. Note that this form of copy is distinct from CreateCopy() in
that it preserves the exact binary files on disk in the new location
while CreateCopy() just attempts to reproduce a new dataset with
essentially the same data as modelled and carried through GDAL.

pfnDelete()
-----------


The delete operations default implementation will be extended to use the
GetFileList() results.

Supporting Functions
--------------------

Some sort of supporting functions should be provided to make it easy to
identify worldfiles, .aux files and .prj files associated with a file.

Drivers Updated
---------------

It is anticipated that a majority of the commonly used drivers will be
updated with custom GetFileList() methods that account for world files
and other idiosyncratic files. A particular emphasis will made to handle
the various formats in gdal/frmts/raw that consist of a header file and
a raw binary file.

Drivers for "one file formats" that are not updated will still use the
default logic which should work fairly well, but might neglect auxiliary
world files.

-  VRT: I do not anticipate updating the VRT driver at this time since
   it gets quite complicated to collect a file list for some kinds of
   virtual files. It is also not exactly clear whether related files
   should be considered "owned" by the virtual dataset or not.
-  AIGRID: I will implement a custom rename operation in an attempt to
   handle this directory oriented format gracefully.

Additional Notes
----------------

-  Subdatasets will generally return an empty file list from
   GetFileList(), and will not be manageable via Rename or Delete though
   a very sophisticated driver could implement these operations.
-  There is no mechanism anticipated to ensure that files are closed
   before they are removed. If an application does not ensure this
   rename/move operations may fail on win32 since it doesn't allow
   rename/delete operations on open files. Things could easily be left
   in an inconsistent state.
-  Datasets without associated files in the file system will return an
   empty file list. This essentially identifies them as "unmanagable".

Implementation Plan
-------------------

This change will be implemented by Frank Warmerdam in trunk in time for
the 1.5.0 release.

SWIG Implications
-----------------

The GDALRenameDataset(), and GDALCopyDatasetFiles() operations on the
driver, and the GetFileList() operation on the dataset will need to be
exposed through SWIG.

Testing
-------

Rename and CopyFiles testing will be added to the regression tests for a
few representative formats. These rename operations will be between one
directory and another, and will not test cross file system copying which
will have to be tested manually.

A small gdalmanage utility will be implemented allowing use and testing
of the identify, rename, copy and delete operations from the commandline
in a convenient fashion.
