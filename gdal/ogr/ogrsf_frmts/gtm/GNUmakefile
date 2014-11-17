

include ../../../GDALmake.opt

OBJ =	ogrgtmdriver.o ogrgtmdatasource.o ogrgtmlayer.o gtm.o gtmwaypointlayer.o gtmtracklayer.o

CPPFLAGS	:=-I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	gtm.h ogr_gtm.h
