GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	= \
	tif_aux.o \
	tif_close.o \
	tif_codec.o \
	tif_compress.o \
	tif_dir.o \
	tif_dirinfo.o \
	tif_dirread.o \
	tif_dirwrite.o \
	tif_dumpmode.o \
	tif_error.o \
	tif_fax3.o \
	tif_fax3sm.o \
	tif_getimage.o \
	tif_jpeg.o \
	tif_flush.o \
	tif_luv.o \
	tif_lzw.o \
	tif_next.o \
	tif_open.o \
	tif_packbits.o \
	tif_pixarlog.o \
	tif_predict.o \
	tif_print.o \
	tif_read.o \
	tif_swab.o \
	tif_strip.o \
	tif_thunder.o \
	tif_tile.o \
	tif_vsi.o \
	tif_version.o \
	tif_warning.o \
	tif_write.o \
	tif_zip.o

O_OBJ	=	$(foreach file,$(OBJ),../../o/$(file))

ALL_C_FLAGS	=	$(CFLAGS) -DZIP_SUPPORT -I../../zlib

ifneq ($(JPEG_SETTING),no)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DJPEG_SUPPORT
endif

ifeq ($(JPEG_SETTING),internal)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -I../../jpeg
endif

default:	$(OBJ)

clean:
	rm -f $(O_OBJ) *.o *.a

update:
	mv port.h port_safe.h
	copymatch.sh ~/osrs/libtiff/libtiff *.c *.h
	mv port_safe.h port.h

install-obj:	$(O_OBJ)

../../o/%.o:	%.o
	cp $*.o ../../o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/port $*.c -o $*.o
