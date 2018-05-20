
OBJ	=	jpeglsdataset.o

include ../../GDALmake.opt

CPPFLAGS	:=	 $(CHARLS_INC) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
