
OBJ	=	zarrdriver.o

include ../../GDALmake.opt

CPPFLAGS :=  -I../mem -I../gtiff $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
