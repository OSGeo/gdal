

include ../../../GDALmake.opt

OBJ	=  geometryreader.o geometrywriter.o ogrflatgeobufdataset.o ogrflatgeobuflayer.o packedrtree.o

CPPFLAGS	:=	-I. -I.. -I../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f $(O_OBJ) *.o *.lo $(O_OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):	ogr_flatgeobuf.h cplerrors.h

