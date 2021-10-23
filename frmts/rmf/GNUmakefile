
include ../../GDALmake.opt

OBJ	=	rmfdataset.o rmflzw.o rmfdem.o rmfjpeg.o

ifneq ($(JPEG_SETTING),no)
CPPFLAGS 	:=	$(CPPFLAGS) -DHAVE_LIBJPEG
endif

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
