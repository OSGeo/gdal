# __init__ for osgeo package.

# making the osgeo package version the same as the gdal version:
import _gdal
__version__ = _gdal.__version__ = _gdal.VersionInfo("RELEASE_NAME") 

