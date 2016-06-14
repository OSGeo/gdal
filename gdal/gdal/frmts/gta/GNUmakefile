
include ../../GDALmake.opt

OBJ	=	gtadataset.o

CPPFLAGS := $(CPPFLAGS) $(NO_NON_VIRTUAL_DTOR_FLAG)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
