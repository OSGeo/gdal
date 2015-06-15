

include ../../../GDALmake.opt

OBJ	=	ogrmongodbdriver.o

CPPFLAGS	:=	-I../generic  $(MONGODB_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
