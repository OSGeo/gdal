GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	= \
	xtiff.o \
	geo_free.o \
	geo_get.o \
	geo_names.o \
	geo_new.o \
	geo_print.o \
	geo_set.o \
	geo_tiffp.o \
	geo_write.o \
	geo_normalize.o \
	geotiff_proj4.o \
	geo_extra.o \
	geo_trans.o

ALL_C_FLAGS	=	-I../libtiff $(CFLAGS)


default:	libgeotiff.a

clean:
	rm -f *.o *.a

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/port $*.c -o $*.o

libgeotiff.a:	$(OBJ)
	ar r libgeotiff.a $(OBJ)

