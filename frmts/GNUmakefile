
GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	gdalallregister.o geotiff.o

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o

%.o:	geotiff/%.cpp
	$(CC) -c $(ALL_C_FLAGS) -Igeotiff/libtiff -Igeotiff/libgeotiff \
		 -I../core geotiff/$*.cpp -o $*.o


gdalallregister.o:	gdalallregister.cpp
	$(CC) -c $(ALL_C_FLAGS) -DGDAL_FORMATS="$(GDAL_FORMATS)" \
		-I$(PCIHOME)/lib -I../core $*.cpp -o $*.o
