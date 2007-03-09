

include ../../GDALmake.opt
include csharp.opt

all: build

BINDING = csharp
SWIG = swig
SWIGARGS = -Wall -I../include -I../include/$(BINDING)

LINK = $(LD_SHARED)
LINK_EXTRAFLAGS = 
OBJ_EXT = .o
ifeq ($(HAVE_LIBTOOL),yes)
LINK = $(LD)
LINK_EXTRAFLAGS = -rpath $(INST_LIB) -no-undefined -version-info $(LIBGDAL_CURRENT):$(LIBGDAL_REVISION):$(LIBGDAL_AGE)
OBJ_EXT = .lo
endif


CSHARP_MODULES = libgdalcsharp.$(SO_EXT) libogrcsharp.$(SO_EXT) libgdalconstcsharp.$(SO_EXT) libosrcsharp.$(SO_EXT)
CSHARP_OBJECTS = const/gdalconst_wrap.$(OBJ_EXT) gdal/gdal_wrap.$(OBJ_EXT) osr/osr_wrap.$(OBJ_EXT) ogr/ogr_wrap.$(OBJ_EXT)

CSC = mcs

clean:
	-rm -f ${CSHARP_MODULES} *$(OBJ_EXT) *.config *.dll *.mdb *.exe

veryclean: clean
	-rm -f *.cs *.cpp *.c const/*.cs const/*.c gdal/*.cs gdal/*.cpp osr/*.cs osr/*.cpp ogr/*.cs ogr/*.cpp

interface:
	$(SWIG) -$(BINDING) -namespace OSGeo.GDAL -dllimport gdalconst_wrap -o const/gdalconst_wrap.c ../include/gdalconst.i
	$(SWIG) $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -namespace OSGeo.GDAL -dllimport gdal_wrap -o gdal/gdal_wrap.cpp ../include/gdal.i
	$(SWIG) $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -namespace OSGeo.OSR -dllimport osr_wrap -o osr/osr_wrap.cpp ../include/osr.i
	$(SWIG) $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -namespace OSGeo.OGR -dllimport osr_wrap -o ogr/osr_wrap.cpp ../include/osr.i
	$(SWIG) $(SWIGARGS) $(SWIGDEFINES) -I$(GDAL_ROOT) -c++ -$(BINDING) -namespace OSGeo.OGR -dllimport ogr_wrap -o ogr/ogr_wrap.cpp ../include/ogr.i

build: ${CSHARP_OBJECTS} ${CSHARP_MODULES} gdal_csharp samples

install:
	@echo "No installation to be done"

$(CSHARP_MODULES): lib%csharp.$(SO_EXT): %_wrap.$(OBJ_EXT)
	$(LINK) $(LDFLAGS) $(CONFIG_LIBS) $< -o $@ $(LINK_EXTRAFLAGS)
	rm -rf $*_csharp.dll.config
	echo "<configuration>" >> $*_csharp.dll.config
	echo "<dllmap dll=\""$*"_wrap\" target=\""$@"\">" >> $*_csharp.dll.config
	echo "</configuration>" >> $*_csharp.dll.config

%$(OBJ_EXT): %.cpp
	$(CXX) $(CFLAGS) $(GDAL_INCLUDE) -c $<

%$(OBJ_EXT): %.cxx
	$(CXX) $(CFLAGS) $(GDAL_INCLUDE) -c $<

%$(OBJ_EXT): %.c
	$(CC) $(CFLAGS) $(GDAL_INCLUDE) -c $<

gdal_csharp:
	$(CSC) /debug:full /target:library /out:ogr_csharp.dll ogr/*.cs
	$(CSC) /debug:full /target:library /out:gdal_csharp.dll gdal/*.cs
	$(CSC) /debug:full /target:library /out:gdalconst_csharp.dll const/*.cs
	$(CSC) /debug:full /target:library /out:osr_csharp.dll osr/*.cs

samples:
	$(CSC) /r:ogr_csharp.dll /out:ogrinfo.exe apps/ogrinfo.cs
	$(CSC) /r:ogr_csharp.dll /out:createdata.exe apps/createdata.cs
	$(CSC) /r:osr_csharp.dll /out:OSRTransform.exe apps/OSRTransform.cs
	$(CSC) /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALRead.exe apps/GDALRead.cs
	$(CSC) /r:gdal_csharp.dll /r:System.Drawing.dll /out:GDALReadDirect.exe apps/GDALReadDirect.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALWrite.exe apps/GDALWrite.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALColorTable.exe apps/GDALColorTable.cs
	$(CSC) /r:ogr_csharp.dll /out:WKT2WKB.exe apps/WKT2WKB.cs
	$(CSC) /r:gdal_csharp.dll /out:ReadXML.exe apps/ReadXML.cs
	$(CSC) /r:gdal_csharp.dll /out:GDALInfo.exe apps/GDALInfo.cs

test:
	LC_ALL=C mono createdata.exe Data pointlayer
	LC_ALL=C mono ogrinfo.exe Data/pointlayer.shp
	LC_ALL=C mono OSRTransform.exe
