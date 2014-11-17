
include ../../GDALmake.opt

OBJ	=	wcsdataset.o httpdriver.o

CPPFLAGS	:=	 $(CPPFLAGS) 

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(OBJ) $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
