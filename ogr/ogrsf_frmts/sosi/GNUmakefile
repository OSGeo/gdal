

include ../../../GDALmake.opt

OBJ	=	ogrsosidriver.o ogrsosidatasource.o ogrsosilayer.o ogrsosidatatypes.o

CPPFLAGS	:=-DLINUX -DUNIX -iquote .. -iquote ../..  $(SOSI_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_sosi.h

