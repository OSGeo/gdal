

II_SYSTEM =	/opt/Ingres/IngresII
INGRES_INC = 	-I$(II_SYSTEM)/ingres/files

include ../../../GDALmake.opt

OBJ	=	ogringresdriver.o ogringresstatement.o ogringresdatasource.o \
		ogringrestablelayer.o ogringreslayer.o

#	ogringresresultlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(INGRES_INC) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)
