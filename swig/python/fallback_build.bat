rem
rem This is a fallback to build the python extensions with Python 2.5 and
rem MSVC 7.1 on windows when the setup.py magic does not work.  It assumes
rem that "python setup.py build" has already created the build/lib... directory
rem and copied in the appropriate .py files.
rem

set PYTHONHOME=C:\OSGeo4W\apps\Python25
set OUTDIR=build\lib.win32-2.5\osgeo

set CLARGS=/c /nologo /Ox /MD /GX /DNDEBUG -I../../port -I../../gcore -I../../alg -I../../ogr/ -I%PYTHONHOME%\include -I%PYTHONHOME%\PC -I%PYTHONHOME%\lib\site-packages\numpy\core\include

cl.exe %CLARGS% extensions\gdal_wrap.cpp
cl.exe %CLARGS% extensions\osr_wrap.cpp
cl.exe %CLARGS% extensions\ogr_wrap.cpp
cl.exe %CLARGS% extensions\gdalconst_wrap.c
cl.exe %CLARGS% extensions\gdal_array_wrap.cpp

set LINKOPTS=/DLL /nologo /INCREMENTAL:NO /LIBPATH:../../ /LIBPATH:%PYTHONHOME%\libs /LIBPATH:%PYTHONHOME%\PCBuild gdal_i.lib

link.exe %LINKOPTS% /EXPORT:init_gdal gdal_wrap.obj /OUT:%OUTDIR%\_gdal.pyd 
link.exe %LINKOPTS% /EXPORT:init_osr  osr_wrap.obj /OUT:%OUTDIR%\_osr.pyd 
link.exe %LINKOPTS% /EXPORT:init_ogr  ogr_wrap.obj /OUT:%OUTDIR%\_ogr.pyd 
link.exe %LINKOPTS% /EXPORT:init_gdalconst gdalconst_wrap.obj /OUT:%OUTDIR%\_gdalconst.pyd 
link.exe %LINKOPTS% /EXPORT:init_gdal_array gdal_array_wrap.obj /OUT:%OUTDIR%\_gdal_array.pyd


