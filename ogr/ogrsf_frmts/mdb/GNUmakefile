

include ../../../GDALmake.opt

OBJ	=	ogrmdbdatasource.o ogrmdblayer.o ogrmdbdriver.o \
		ogrmdbjackcess.o

CPPFLAGS	:=	-iquote ..  $(JAVA_INC) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_mdb.h
