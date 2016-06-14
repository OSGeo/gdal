
include ../../../GDALmake.opt

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
	geo_trans.o \
	geo_simpletags.o

O_OBJ	=	$(foreach file,$(OBJ),../../o/$(file))

ALL_C_FLAGS	=	$(CPPFLAGS) $(CFLAGS) -DDONT_DEPRECATE_SPRINTF

ifeq ($(TIFF_SETTING),internal)
ALL_C_FLAGS	:=	-I../libtiff $(ALL_C_FLAGS)
ifeq ($(RENAME_INTERNAL_LIBTIFF_SYMBOLS),yes)
ALL_C_FLAGS	:=	-DRENAME_INTERNAL_LIBTIFF_SYMBOLS $(ALL_C_FLAGS)
endif
endif
ifeq ($(RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS),yes)
ALL_C_FLAGS	:=	-DRENAME_INTERNAL_LIBGEOTIFF_SYMBOLS $(ALL_C_FLAGS)
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(O_OBJ) *.o *.a

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

../../o/%.$(OBJ_EXT):	%.c
	$(CC) -c -I../../port $(ALL_C_FLAGS) $< -o $@

#
# Updating to the latest libgeotiff involves copying all matching source
# except for a few files that hook to GDALs own CPL services.
#
import:
	@if test ! -d ~/libgeotiff ; then \
	  echo reimport requires libgeotiff checked out ~/libgeotiff ; \
	  exit 1; \
	fi

	rm -rf safe
	mkdir safe
	mv cpl_serv.h geo_config.h safe

	copymatch.sh ~/libgeotiff *.cpp *.c *.h *.inc
	copymatch.sh ~/libgeotiff/libxtiff xtiff*.c xtiffio.h

	mv safe/* .
	rm -rf safe

	@echo
	@echo 'Now do something like:'
	@echo '% cvs commit -m "updated to libgeotiff 1.1.x"'
	@echo
