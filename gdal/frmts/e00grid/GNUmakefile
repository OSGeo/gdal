
include ../../GDALmake.opt

OBJ	=	e00griddataset.o

CPPFLAGS	:=	 $(CPPFLAGS) $(XTRA_OPT)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o

all:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       e00read.c

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
