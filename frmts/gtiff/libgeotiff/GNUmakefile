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

O_OBJ	=	$(foreach file,$(OBJ),../../o/$(file))

ALL_C_FLAGS	=	$(CFLAGS)

ifeq ($(TIFF_SETTING),internal)
ALL_C_FLAGS	=	-I../libtiff $(ALL_C_FLAGS)
endif


default:	$(OBJ)

clean:
	rm -f $(O_OBJ) *.o *.a

install-obj:	$(O_OBJ)

../../o/%.o:	%.o
	cp $*.o ../../o

%.o:	%.c
	$(CC) -c -I$(GDAL_ROOT)/port $(ALL_C_FLAGS) $*.c -o $*.o
