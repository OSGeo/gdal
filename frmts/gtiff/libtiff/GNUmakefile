
include ../../GDALmake.opt

OBJ	= \
	tif_aux.o \
	tif_close.o \
	tif_codec.o \
	tif_color.o \
	tif_compress.o \
	tif_dir.o \
	tif_dirinfo.o \
	tif_dirread.o \
	tif_dirwrite.o \
	tif_dumpmode.o \
	tif_error.o \
	tif_extension.o \
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

ALL_C_FLAGS =	$(CFLAGS) $(CPPFLAGS) \
		-DZIP_SUPPORT -DPIXARLOG_SUPPORT -I../../zlib

ifneq ($(JPEG_SETTING),no)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DJPEG_SUPPORT
endif

ifeq ($(JPEG_SETTING),internal)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -I../../jpeg/libjpeg
endif

default:	$(OBJ)

clean:
	rm -f $(O_OBJ) *.o *.a

import:
	@if test ! -d ~/libtiff ; then \
	  echo reimport requires libtiff checked out ~/libtiff ; \
	  exit 1; \
	fi

	mv tif_config.h tif_config_safe.h
	mv tiffconf.h tiffconf_safe.h
	copymatch.sh ~/libtiff/libtiff *.c *.h
	mv tif_config_safe.h tif_config.h
	mv tiffconf_safe.h tiffconf.h

	@echo
	@echo 'Now do something like:'
	@echo '% cvs commit -m "updated to libtiff 3.6.0"'
	@echo

install-obj:	$(O_OBJ)

../../o/%.o:	%.c
	$(CC) -c -I../../port $(ALL_C_FLAGS) $< -o $@
