

include ../../../GDALmake.opt

OBJ	=	ogridbdatasource.o ogridblayer.o ogridbdriver.o \
		ogridbtablelayer.o ogridbselectlayer.o

IDB_DEFS =	-DIT_DLLIB -DIT_DO_NOT_SIMULATE_BOOL

CPPFLAGS :=	-I.. -I../..  $(IDB_INC) $(IDB_DEFS) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
