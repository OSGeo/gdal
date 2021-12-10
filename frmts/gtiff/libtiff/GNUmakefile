
include ../../../GDALmake.opt

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
	tif_jpeg_12.o \
	tif_flush.o \
	tif_luv.o \
	tif_lzw.o \
	tif_next.o \
	tif_ojpeg.o \
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
	tif_zip.o \
	tif_lzma.o \
	tif_zstd.o \
	tif_webp.o \
	tif_lerc.o

O_OBJ	=	$(foreach file,$(OBJ),../../o/$(file))

# Note: HOST_FILLORDER value is set arbitrarily. This isn't used by GDAL
ALL_C_FLAGS =	$(CFLAGS_NOFTRAPV) $(CPPFLAGS) -DDONT_DEPRECATE_SPRINTF -DHOST_FILLORDER=FILLORDER_LSB2MSB

ifeq ($(LIBZ_SETTING),internal)
XTRA_OPT =      -I../../zlib
else
XTRA_OPT =
endif

ifeq ($(RENAME_INTERNAL_LIBTIFF_SYMBOLS),yes)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DRENAME_INTERNAL_LIBTIFF_SYMBOLS
endif

ifneq ($(LIBZ_SETTING),no)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DPIXARLOG_SUPPORT -DZIP_SUPPORT $(XTRA_OPT)
endif

ifneq ($(JPEG_SETTING),no)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DJPEG_SUPPORT -DOJPEG_SUPPORT
# jpeg9: -DHAVE_BOOLEAN -Dboolean=int
endif

ifeq ($(JPEG_SETTING),internal)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -I../../jpeg/libjpeg
ifeq ($(RENAME_INTERNAL_LIBJPEG_SYMBOLS),yes)
ALL_C_FLAGS	:=	$(ALL_C_FLAGS) -DRENAME_INTERNAL_LIBJPEG_SYMBOLS
endif
endif

ifeq ($(TIFF_JPEG12_ENABLED),yes)
ALL_C_FLAGS	:=      $(ALL_C_FLAGS) -DJPEG_DUAL_MODE_8_12
EXTRA_DEP	=	libjpeg12src
endif

ifeq ($(LIBLZMA_SETTING),yes)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DLZMA_SUPPORT
endif

ifeq ($(ZSTD_SETTING),yes)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DZSTD_SUPPORT
endif

ifeq ($(WEBP_SETTING),yes)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DWEBP_SUPPORT
endif

ifeq ($(LIBDEFLATE_SETTING),yes)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DLIBDEFLATE_SUPPORT
endif

ifeq ($(HAVE_LERC),internal)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DLERC_SUPPORT -I../../../third_party/LercLib
endif

ifeq ($(HAVE_LERC),external)
ALL_C_FLAGS 	:=	$(ALL_C_FLAGS) -DLERC_SUPPORT $(LERC_INCLUDE)
endif

default:	$(EXTRA_DEP) $(OBJ:.o=.$(OBJ_EXT))

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
	@echo '% svn commit -m "updated to libtiff 3.6.0"'
	@echo

install-obj:	$(EXTRA_DEP) $(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):	%.c t4.h tif_config.h tif_dir.h tif_fax3.h tif_predict.h tiff.h tiffconf.h tiffio.h tiffiop.h tiffvers.h uvcode.h
	$(CC) -c $(ALL_C_FLAGS) $< -o $@

libjpeg12src:
	(cd ../../jpeg; $(MAKE) libjpeg12/jcapimin12.c)
