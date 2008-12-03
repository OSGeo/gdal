
include ../../../GDALmake.opt

OBJ	=	ogringresdriver.o ogringresstatement.o ogringresdatasource.o \
		ogringrestablelayer.o ogringreslayer.o ogringresresultlayer.o

#	ogringresresultlayer.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(INGRES_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
