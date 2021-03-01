
include ../../GDALmake.opt
include csharp.opt

all: build

BINDING = csharp
include ../SWIGmake.base

SWIGARGS = -Wall -I../include -I../include/$(BINDING)

LINK = $(LD_SHARED)
LINK_EXTRAFLAGS =
OBJ_EXT = o
ifeq ($(HAVE_LIBTOOL),yes)
LINK = $(LD)
LINK_EXTRAFLAGS = -rpath $(INST_LIB) -no-undefined -version-info $(LIBGDAL_CURRENT):$(LIBGDAL_REVISION):$(LIBGDAL_AGE)
OBJ_EXT = lo
endif

SUPPRESSW = -Wno-missing-prototypes -Wno-missing-declarations -Wno-deprecated-declarations -Wno-unused-parameter -Wno-unused-function

CSHARP_MODULES = libgdalcsharp.$(SO_EXT) libogrcsharp.$(SO_EXT) libgdalconstcsharp.$(SO_EXT) libosrcsharp.$(SO_EXT)
CSHARP_OBJECTS = const/gdalconst_wrap.$(OBJ_EXT) gdal/gdal_wrap.$(OBJ_EXT) osr/osr_wrap.$(OBJ_EXT) ogr/ogr_wrap.$(OBJ_EXT)

CSC = mcs

clean:
	-rm -f ${CSHARP_MODULES} *.$(OBJ_EXT) *.config *.dll *.mdb *.exe

veryclean: clean
	-rm -f -R const/*.cs const/*.c gdal/*.cs gdal/*.cpp osr/*.cs osr/*.cpp ogr/*.cs ogr/*.cpp Data

generate: interface
interface:
	$(SWIG) -DSWIG2_CSHARP -$(BINDING) -namespace OSGeo.GDAL -dllimport gdalconst_wrap -o const/gdalconst_wrap.c ../include/gdalconst.i
	$(SWIG) -DSWIG2_CSHARP $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -namespace OSGeo.GDAL -dllimport gdal_wrap -o gdal/gdal_wrap.cpp ../include/gdal.i
	$(SWIG) -DSWIG2_CSHARP $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -namespace OSGeo.OSR -dllimport osr_wrap -o osr/osr_wrap.cpp ../include/osr.i
	$(SWIG) -DSWIG2_CSHARP $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -namespace OSGeo.OGR -dllimport ogr_wrap -o ogr/ogr_wrap.cpp ../include/ogr.i

build: ${CSHARP_OBJECTS} ${CSHARP_MODULES} gdal_csharp samples

sign:
	sn -k gdal.snk

install:
	@echo "No installation to be done"

$(CSHARP_MODULES): lib%csharp.$(SO_EXT): %_wrap.$(OBJ_EXT)
	$(LINK) $(LDFLAGS) $(CONFIG_LIBS) $< -o $@ $(GDAL_SLIB_LINK) $(LINK_EXTRAFLAGS)
	rm -rf $*_csharp.dll.config
	echo "<configuration>" >> $*_csharp.dll.config
	echo "<dllmap dll=\""$*"_wrap\" target=\""$*csharp"\" />" >> $*_csharp.dll.config
	echo "</configuration>" >> $*_csharp.dll.config

%.$(OBJ_EXT): %.cpp
	$(CXX) $(CFLAGS) $(SUPPRESSW) $(GDAL_INCLUDE) -c $<

%.$(OBJ_EXT): %.cxx
	$(CXX) $(CFLAGS) $(SUPPRESSW) $(GDAL_INCLUDE) -c $<

%.$(OBJ_EXT): %.c
	$(CC) $(CFLAGS) $(SUPPRESSW) $(GDAL_INCLUDE) -c $<

gdal_csharp:
	$(CSC) /unsafe /debug:full /target:library /out:osr_csharp.dll osr/*.cs AssemblyInfo.cs
	$(CSC) /unsafe /debug:full /target:library /out:ogr_csharp.dll /r:osr_csharp.dll ogr/*.cs AssemblyInfo.cs
	$(CSC) /unsafe /debug:full /target:library /out:gdal_csharp.dll /r:ogr_csharp.dll /r:osr_csharp.dll gdal/*.cs AssemblyInfo.cs
	$(CSC) /unsafe /debug:full /target:library /out:gdalconst_csharp.dll const/*.cs AssemblyInfo.cs

samples:
	$(CSC) /r:ogr_csharp.dll /r:osr_csharp.dll /out:ogrinfo.exe apps/ogrinfo.cs
	$(CSC) /r:ogr_csharp.dll /r:osr_csharp.dll /out:createdata.exe apps/createdata.cs
	$(CSC) /r:osr_csharp.dll /out:OSRTransform.exe apps/OSRTransform.cs
	$(CSC) /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALRead.exe apps/GDALRead.cs
	$(CSC) /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALReadDirect.exe apps/GDALReadDirect.cs
	$(CSC) /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALAdjustContrast.exe apps/GDALAdjustContrast.cs
	$(CSC) /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALDatasetRasterIO.exe apps/GDALDatasetRasterIO.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALWrite.exe apps/GDALWrite.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALDatasetWrite.exe apps/GDALDatasetWrite.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALColorTable.exe apps/GDALColorTable.cs
	$(CSC) /r:ogr_csharp.dll /out:WKT2WKB.exe apps/WKT2WKB.cs
	$(CSC) /r:ogr_csharp.dll /out:OGRGEOS.exe apps/OGRGEOS.cs
	$(CSC) /r:gdal_csharp.dll /out:ReadXML.exe apps/ReadXML.cs
	$(CSC) /r:gdal_csharp.dll /r:osr_csharp.dll /out:GDALInfo.exe apps/GDALInfo.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALOverviews.exe apps/GDALOverviews.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALCreateCopy.exe apps/GDALCreateCopy.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALGetHistogram.exe apps/GDALGetHistogram.cs

vagrant_safe_test:
	LC_ALL=C mono createdata.exe Data pointlayer
	LC_ALL=C mono ogrinfo.exe Data/pointlayer.shp
	LC_ALL=C mono GDALWrite.exe Data/sample.tif
	LC_ALL=C mono GDALDatasetWrite.exe Data/sample1.tif
	LC_ALL=C mono GDALCreateCopy.exe Data/sample.tif Data/sample2.tif
	LC_ALL=C mono GDALOverviews.exe Data/sample.tif NEAREST 2 4
	LC_ALL=C mono GDALInfo.exe Data/sample.tif

test: vagrant_safe_test
	LC_ALL=C mono OSRTransform.exe

.PHONY: all clean veryclean generate interface build sign install gdal_csharp samples vagrant_safe_test test
