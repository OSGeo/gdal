

include ../../GDALmake.opt

GDAL_OBJ	=	nitfdataset.o rpftocdataset.o nitfwritejpeg.o nitfwritejpeg_12.o
NITFLIB_OBJ	=	nitffile.o nitfimage.o mgrs.o nitfaridpcm.o \
			nitfbilevel.o rpftocfile.o

CPPFLAGS	:=	-I../vrt $(GDAL_INCLUDE) $(CPPFLAGS)

ifeq ($(TIFF_SETTING),internal)
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
ifeq ($(JPEG_SETTING),internal)
PREDEPEND 	=	libjpeg12
endif
endif

OBJ = $(GDAL_OBJ) $(NITFLIB_OBJ)

default:	$(PREDEPEND) $(OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	../../gcore/gdal_proxy.h nitflib.h nitfwritejpeg.cpp

clean:
	rm -f *.o $(O_OBJ) nitfdump$(EXE)

nitfdump$(EXE):	nitfdump.$(OBJ_EXT)
	$(LD) $(LDFLAGS) nitfdump.$(OBJ_EXT) $(CONFIG_LIBS) -o nitfdump$(EXE)

install-obj:	$(PREDEPEND) $(O_OBJ:.o=.$(OBJ_EXT))

libjpeg12:
	(cd ../jpeg; $(MAKE) libjpeg12/jcapimin12.c)
