GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	gdalopen.o gdaldrivermanager.o gdaldriver.o gdaldataset.o \
		gdalrasterband.o gdal_misc.o rasterio.o gdalrasterblock.o \
		gdalprojdef.o gdalcolortable.o gdalmajorobject.o overview.o \
		gdaldefaultoverviews.o

default:	$(OBJ)

clean:
	rm -f *.o

docs:	
	(cd ..; $(MAKE) docs)

%.o:	%.c
	$(CC) -c $(CFLAGS) $(GDAL_INCLUDE) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) -c $(CXXFLAGS) $(GDAL_INCLUDE) $*.cpp -o $*.o
