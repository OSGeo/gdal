

include ../../GDALmake.opt

GDAL_OBJ	=	nitfdataset.o rpftocdataset.o
NITFLIB_OBJ	=	nitffile.o nitfimage.o mgrs.o nitfaridpcm.o \
			nitfbilevel.o rpftocfile.o

OBJ = $(GDAL_OBJ) $(NITFLIB_OBJ)

CPPFLAGS	:=	-I../gtiff/libtiff -I../vrt $(GDAL_INCLUDE) $(CPPFLAGS)

ifneq ($(JPEG_SETTING),no)
CPPFLAGS	:=	$(CPPFLAGS) -DJPEG_SUPPORTED
endif

ifeq ($(JPEG_SETTING),internal)
CPPFLAGS	:=	$(CPPFLAGS) -I../jpeg/libjpeg
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	../../gcore/gdal_proxy.h nitflib.h

clean:
	rm -f *.o $(O_OBJ) nitfdump$(EXE)

nitfdump$(EXE):	nitfdump.$(OBJ_EXT)
	$(LD) $(LDFLAGS) nitfdump.$(OBJ_EXT) $(CONFIG_LIBS) -o nitfdump$(EXE)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
