
include ../../GDALmake.opt

OBJ	=	heifdataset.o

CPPFLAGS	:=	 $(HEIF_CFLAGS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
