GDAL_ROOT	=	..

include ../GDALmake.opt

INCLUDE		=	$(GDAL_INCLUDE) -I$(GDAL_ROOT)/frmts

default:	gdalinfo gdal_translate

lib-depend:
	(cd ../core ; $(MAKE) )
	(cd ../port ; $(MAKE) )

gdalinfo:	gdalinfo.o 
	$(CXX) $(LNK_FLAGS) gdalinfo.o $(XTRAOBJ) $(GDAL_LIBS) $(LIBS) \
		 -o gdalinfo

gdal_translate:	gdal_translate.o
	$(CXX) $(LNK_FLAGS) gdal_translate.o $(XTRAOBJ) $(GDAL_LIBS) $(LIBS) \
		-o gdal_translate

gdal2raw:	gdal2raw.o
	$(CXX) $(LNK_FLAGS) gdal2raw.o $(XTRAOBJ) $(GDAL_LIBS) $(LIBS) \
		-o gdal2raw

gdal_chksum:	gdal_chksum.o
	$(CXX) $(LNK_FLAGS) gdal_chksum.o $(GDAL_LIBS) $(LIBS) -o gdal_chksum

clean:
	rm -f *.o gdalinfo core

%.o:	%.c
	$(CC) $(INCLUDE) -c $(CFLAGS) $*.c -o $*.o

%.o:	%.cpp
	$(CXX) $(INCLUDE) -c $(CFLAGS) $*.cpp -o $*.o

