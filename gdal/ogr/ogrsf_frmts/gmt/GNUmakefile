
include ../../../GDALmake.opt

OBJ	=	ogrgmtdriver.o ogrgmtdatasource.o ogrgmtlayer.o

CPPFLAGS	:=	-iquote .. -iquote ../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
