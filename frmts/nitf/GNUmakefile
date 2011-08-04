

include ../../GDALmake.opt

GDAL_OBJ =	nitfdataset.o rpftocdataset.o nitfwritejpeg.o \
		nitfwritejpeg_12.o nitfrasterband.o ecrgtocdataset.o
NITFLIB_OBJ =	nitffile.o nitfimage.o mgrs.o nitfaridpcm.o \
		nitfbilevel.o rpftocfile.o nitfdes.o nitf_gcprpc.o

CPPFLAGS	:=	-I../vrt $(GDAL_INCLUDE) $(CPPFLAGS)

ifeq ($(TIFF_SETTING),internal)
ifeq ($(RENAME_INTERNAL_LIBTIFF_SYMBOLS),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DRENAME_INTERNAL_LIBTIFF_SYMBOLS
endif
CPPFLAGS	:=	$(CPPFLAGS) -I../gtiff/libtiff
endif

ifneq ($(JPEG_SETTING),no)
CPPFLAGS	:=	$(CPPFLAGS) -DJPEG_SUPPORTED
endif

ifeq ($(JPEG_SETTING),internal)
CPPFLAGS	:=	$(CPPFLAGS) -I../jpeg/libjpeg
endif

ifeq ($(JPEG12_ENABLED),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DJPEG_DUAL_MODE_8_12
PREDEPEND 	=	libjpeg12
endif

OBJ = $(GDAL_OBJ) $(NITFLIB_OBJ)

default:	$(PREDEPEND) $(OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	../../gcore/gdal_proxy.h  ../vrt/vrtdataset.h nitflib.h nitfwritejpeg.cpp

clean:
	rm -f *.o $(O_OBJ) nitfdump$(EXE)

nitfdump$(EXE):	nitfdump.$(OBJ_EXT)
	$(LD) $(LDFLAGS) nitfdump.$(OBJ_EXT) $(CONFIG_LIBS) -o nitfdump$(EXE)

install-obj:	$(PREDEPEND) $(O_OBJ:.o=.$(OBJ_EXT))

libjpeg12:
	(cd ../jpeg; $(MAKE) libjpeg12/jcapimin12.c)
