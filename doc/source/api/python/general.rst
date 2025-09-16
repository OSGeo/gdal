..
   The documentation displayed on this page is automatically generated from
   Python docstrings. See https://gdal.org/development/dev_documentation.html
   for information on updating this content.

.. _python_general:

General API
===========

- `Configuration Management`_
- `Data Type Information`_
- `Error Handling`_
- `File Management`_


Configuration Management
------------------------

.. autofunction:: osgeo.gdal.config_option

.. autofunction:: osgeo.gdal.config_options

.. autofunction:: osgeo.gdal.ClearCredentials

.. autofunction:: osgeo.gdal.ClearPathSpecificOptions

.. autofunction:: osgeo.gdal.GetCacheMax

.. autofunction:: osgeo.gdal.GetCacheUsed

.. autofunction:: osgeo.gdal.GetConfigOption

.. autofunction:: osgeo.gdal.GetConfigOptions

.. autofunction:: osgeo.gdal.GetCredential

.. autofunction:: osgeo.gdal.GetGlobalConfigOption

.. autofunction:: osgeo.gdal.GetNumCPUs

.. autofunction:: osgeo.gdal.GetPathSpecificOption

.. autofunction:: osgeo.gdal.GetThreadLocalConfigOption

.. autofunction:: osgeo.gdal.GetUsablePhysicalRAM

.. autofunction:: osgeo.gdal.HasThreadSupport

.. autofunction:: osgeo.gdal.SetCacheMax

.. autofunction:: osgeo.gdal.SetConfigOption

.. autofunction:: osgeo.gdal.SetCredential

.. autofunction:: osgeo.gdal.SetPathSpecificOption

.. autofunction:: osgeo.gdal.SetThreadLocalConfigOption

.. autofunction:: osgeo.gdal.VersionInfo

Data Type Information
---------------------

.. autofunction:: osgeo.gdal.DataTypeIsComplex

.. autofunction:: osgeo.gdal.DataTypeUnion

.. autofunction:: osgeo.gdal.DataTypeUnionWithValue

.. autofunction:: osgeo.gdal.GetDataTypeByName

.. autofunction:: osgeo.gdal.GetDataTypeName

.. autofunction:: osgeo.gdal.GetDataTypeSize

.. autofunction:: osgeo.gdal.GetDataTypeSizeBits

.. autofunction:: osgeo.gdal.GetDataTypeSizeBytes

Error Handling
--------------

.. autofunction:: osgeo.gdal.ConfigurePythonLogging

.. autofunction:: osgeo.gdal.Debug

.. autofunction:: osgeo.gdal.DontUseExceptions

.. autofunction:: osgeo.gdal.Error

.. autofunction:: osgeo.gdal.ErrorReset

.. autoclass:: osgeo.gdal.ExceptionMgr

.. autofunction:: osgeo.gdal.GetErrorCounter

.. autofunction:: osgeo.gdal.GetLastErrorMsg

.. autofunction:: osgeo.gdal.GetLastErrorNo

.. autofunction:: osgeo.gdal.GetLastErrorType

.. autofunction:: osgeo.gdal.GetUseExceptions

.. autofunction:: osgeo.gdal.PopErrorHandler

.. autofunction:: osgeo.gdal.PushErrorHandler

.. autofunction:: osgeo.gdal.quiet_errors

.. autofunction:: osgeo.gdal.quiet_warnings

.. autofunction:: osgeo.gdal.SetCurrentErrorHandlerCatchDebug

.. autofunction:: osgeo.gdal.SetErrorHandler

.. autofunction:: osgeo.gdal.UseExceptions


File Management
---------------

osgeo.gdal_fsspec module
++++++++++++++++++++++++

.. automodule:: osgeo.gdal_fsspec
   :members:
   :undoc-members:
   :show-inheritance:
   :noindex:

osgeo.gdal.VSIFile class
++++++++++++++++++++++++

.. autoclass:: osgeo.gdal.VSIFile
   :members:
   :undoc-members:
   :noindex:

Low level functions
+++++++++++++++++++

.. autofunction:: osgeo.gdal.CloseDir

.. autofunction:: osgeo.gdal.CopyFile

.. autofunction:: osgeo.gdal.CopyFileRestartable

.. autoclass:: osgeo.gdal.DirEntry
   :members:
   :undoc-members:

.. autofunction:: osgeo.gdal.FileFromMemBuffer

.. autofunction:: osgeo.gdal.FindFile

.. autofunction:: osgeo.gdal.GetFileMetadata

.. autofunction:: osgeo.gdal.GetFileSystemOptions

.. autofunction:: osgeo.gdal.GetFileSystemsPrefixes

.. autofunction:: osgeo.gdal.Mkdir

.. autofunction:: osgeo.gdal.MkdirRecursive

.. autofunction:: osgeo.gdal.Move

.. autofunction:: osgeo.gdal.MoveFile

.. autofunction:: osgeo.gdal.OpenDir

.. autofunction:: osgeo.gdal.ReadDir

.. autofunction:: osgeo.gdal.ReadDirRecursive

.. autofunction:: osgeo.gdal.Rename

.. autofunction:: osgeo.gdal.Rmdir

.. autofunction:: osgeo.gdal.RmdirRecursive

.. autofunction:: osgeo.gdal.SetFileMetadata

.. autofunction:: osgeo.gdal.Sync

.. autofunction:: osgeo.gdal.Unlink

.. autofunction:: osgeo.gdal.UnlinkBatch
