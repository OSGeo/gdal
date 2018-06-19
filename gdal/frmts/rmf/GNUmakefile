
include ../../GDALmake.opt

OBJ	=	rmfdataset.o rmflzw.o rmfdem.o rmfjpeg.o

ifneq ($(JPEG_SETTING),no)
CPPFLAGS 	:=	$(CPPFLAGS) -I../jpeg -DHAVE_LIBJPEG
endif

ifeq ($(JPEG_SETTING),internal)
CPPFLAGS 	:=	$(CPPFLAGS) -I../jpeg/libjpeg
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
