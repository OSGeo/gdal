
include ../../GDALmake.opt

OBJ	=	rasdamandataset.o

CPPFLAGS	:=	 $(CPPFLAGS) $(RASDAMAN_INC)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
