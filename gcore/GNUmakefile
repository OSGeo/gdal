GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	gdalopen.o gdaldrivermanager.o gdaldriver.o gdaldataset.o \
		gdalrasterband.o gdal_misc.o

default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.c
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CC) -c $(GDAL_INCLUDE) $(CFLAGS) $*.cpp -o $*.o

