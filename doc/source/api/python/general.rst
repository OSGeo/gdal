.. _python_general:

Python General API
==================

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

.. autofunction:: osgeo.gdal.SetCurrentErrorHandlerCatchDebug

.. autofunction:: osgeo.gdal.SetErrorHandler

.. autofunction:: osgeo.gdal.UseExceptions


File Management
---------------

.. autofunction:: osgeo.gdal.CloseDir

.. autofunction:: osgeo.gdal.CopyFile

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

.. autofunction:: osgeo.gdal.OpenDir

.. autofunction:: osgeo.gdal.ReadDir

.. autofunction:: osgeo.gdal.ReadDirRecursive

.. autofunction:: osgeo.gdal.Rename

.. autofunction:: osgeo.gdal.Rmdir

.. autofunction:: osgeo.gdal.RmdirRecursive

.. autofunction:: osgeo.gdal.SetFileMetadata

.. autofunction:: osgeo.gdal.Unlink

.. autofunction:: osgeo.gdal.UnlinkBatch
