
GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	gdalallregister.o geotiff.o gxfopen.o gxfdataset.o

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o

%.o:	geotiff/%.cpp
	$(CC) -c $(ALL_C_FLAGS) -Igeotiff/libtiff -Igeotiff/libgeotiff \
		 -I../core geotiff/$*.cpp -o $*.o

%.o:	gxf/%.cpp
	$(CC) -c $(ALL_C_FLAGS) -Igxf -I../core gxf/$*.cpp -o $*.o

%.o:	gxf/%.c
	$(CC) -c $(ALL_C_FLAGS) -Igxf -I../core gxf/$*.c -o $*.o


gdalallregister.o:	gdalallregister.cpp
	$(CC) -c $(ALL_C_FLAGS) -DGDAL_FORMATS="$(GDAL_FORMATS)" \
		-I$(PCIHOME)/lib -I../core $*.cpp -o $*.o
