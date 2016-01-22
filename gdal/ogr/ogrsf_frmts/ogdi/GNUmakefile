

include ../../../GDALmake.opt

OBJ	=	ogrogdidriver.o ogrogdidatasource.o ogrogdilayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(OGDI_INCLUDE) $(PROJ_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):		ogrogdi.h

clean:
	rm -f *.o $(O_OBJ)
