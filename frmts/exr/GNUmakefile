
include ../../GDALmake.opt

OBJ	=	exrdataset.o

CPPFLAGS	:=	 $(EXR_CFLAGS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
