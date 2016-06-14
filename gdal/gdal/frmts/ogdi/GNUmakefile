
include ../../GDALmake.opt

OBJ	=	ogdidataset.o

CPPFLAGS	:=	 $(OGDI_INCLUDE) $(PROJ_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
