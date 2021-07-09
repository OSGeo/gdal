

include ../../../GDALmake.opt

OBJ	=	ogrpdsdriver.o ogrpdsdatasource.o ogrpdslayer.o

CPPFLAGS	:=	-iquote .. -iquote ../.. -iquote ../../../frmts/pds  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_pds.h