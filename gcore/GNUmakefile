
include ../GDALmake.opt

OBJ	=	gdalopeninfo.o gdaldrivermanager.o gdaldriver.o gdaldataset.o \
		gdalrasterband.o gdal_misc.o rasterio.o gdalrasterblock.o \
		gdalcolortable.o gdalmajorobject.o overview.o \
		gdaldefaultoverviews.o gdalpamdataset.o gdalpamrasterband.o \
		gdaljp2metadata.o gdaljp2box.o gdalmultidomainmetadata.o \
		gdal_rat.o

# Enable the following if you want to use MITAB's code to convert
# .tab coordinate systems into well known text.  But beware that linking
# against static libraries becomes more complicated because of the odd
# call out.

ifeq ($(OGR_ENABLED),yes)
CXXFLAGS 	:=	-DHAVE_MITAB $(CXXFLAGS)
endif

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	$(RM) *.o

docs:	
	(cd ..; $(MAKE) docs)

gdal_misc.o:	gdal_misc.cpp gdal_version.h

gdaldrivermanager.o:	gdaldrivermanager.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) -DINST_DATA=\"$(INST_DATA)\" \
		$*.cpp -o $*.o

install:
	for f in *.h ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done
