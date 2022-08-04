
include ../../GDALmake.opt

OBJ	=	usgsdemdataset.o usgsdem_create.o

CPPFLAGS	:=	 -iquote ../../alg -iquote ../mem $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
