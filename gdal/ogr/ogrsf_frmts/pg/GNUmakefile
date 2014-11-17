

include ../../../GDALmake.opt

OBJ	=	ogrpgdriver.o ogrpgdatasource.o ogrpglayer.o ogrpgtablelayer.o\
		ogrpgresultlayer.o ogrpgutility.o

CPPFLAGS	:=	 $(PG_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_pg.h ogrpgutility.h