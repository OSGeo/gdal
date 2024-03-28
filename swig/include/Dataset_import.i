#if defined(SWIGCSHARP)
%module Gdal
#elif defined(SWIGPYTHON)
%module (package="osgeo") gdal
#else
%module gdal
#endif

%include "Dataset.i"
