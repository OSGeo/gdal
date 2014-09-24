

include ../../../GDALmake.opt

OBJ	=	aodriver.o aoatasource.o aolayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(AO_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_ao.h

install-obj:    $(O_OBJ:.o=.$(OBJ_EXT)) 
