GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	gdalopen.o gdaldrivermanager.o gdaldriver.o gdaldataset.o \
		gdalrasterband.o gdal_misc.o rasterio.o gdalrasterblock.o \
		gdalcolortable.o gdalmajorobject.o overview.o \
		gdaldefaultoverviews.o

default:	$(OBJ)

clean:
	rm -f *.o

docs:	
	(cd ..; $(MAKE) docs)

gdaldrivermanager.o:	gdaldrivermanager.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) -DINST_DATA=\"$(INST_DATA)\" \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $*.cpp -o $*.o

install:
	for f in *.h ; do $(INSTALL) $$f $(INST_INCLUDE) ; done
