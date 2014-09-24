

include ../../../GDALmake.opt

OBJ	=	ogrpcidskdriver.o ogrpcidskdatasource.o ogrpcidsklayer.o

ifeq ($(PCIDSK_SETTING),internal)
PCIDSK_INCLUDE = -I../../../frmts/pcidsk/sdk
endif

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(PCIDSK_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_pcidsk.h
