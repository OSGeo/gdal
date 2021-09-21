

include ../../../GDALmake.opt

OBJ	=	ogrmssqlspatialdatasource.o ogrmssqlspatiallayer.o ogrmssqlspatialtablelayer.o ogrmssqlspatialselectlayer.o ogrmssqlspatialdriver.o ogrmssqlgeometryparser.o ogrmssqlgeometryvalidator.o ogrmssqlgeometrywriter.o

CPPFLAGS	:=	 -iquote .. -iquote ../.. $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
