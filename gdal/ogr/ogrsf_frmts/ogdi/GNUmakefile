

include ../../../GDALmake.opt

OBJ	=	ogrogdidriver.o ogrogdidatasource.o ogrogdilayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(OGDI_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

$(OBJ):		ogrogdi.h

clean:
	rm -f *.o $(O_OBJ)
