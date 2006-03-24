

include ../../../GDALmake.opt

OBJ	=	ogrpgdriver.o ogrpgdatasource.o ogrpglayer.o ogrpgtablelayer.o\
		ogrpgresultlayer.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(PG_INC) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
