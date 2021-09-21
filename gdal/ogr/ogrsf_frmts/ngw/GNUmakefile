
include ../../../GDALmake.opt

OBJ	:= \
	ogrngwdriver.o \
	gdalngwdataset.o \
	ogrngwlayer.o \
	ngw_api.o

CPPFLAGS	:=	-iquote . -iquote .. -iquote ../.. $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f *~

$(O_OBJ):	ogr_ngw.h
