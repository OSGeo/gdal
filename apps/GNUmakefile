GDAL_ROOT	=	..

include ../GDALmake.opt

INCLUDE		=	$(GDAL_INCLUDE) -I$(GDAL_ROOT)/frmts

default:	gdalinfo

lib-depend:
	(cd ../core ; $(MAKE) )

gdalinfo:	gdalinfo.o lib-depend
	$(CC) gdalinfo.o $(GDAL_LIBS) $(LIBS) -o gdalinfo

clean:
	rm -f *.o gdalinfo core

%.o:	%.c
	$(CC) $(INCLUDE) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CC) $(INCLUDE) -c $(CFLAGS) $*.cpp -o $*.o

