

include ../../../GDALmake.opt

OBJ	=	ogridrisidriver.o ogridrisidatasource.o ogridrisilayer.o

CPPFLAGS	:=	-I.. -I../.. -I../../../frmts/idrisi  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_idrisi.h