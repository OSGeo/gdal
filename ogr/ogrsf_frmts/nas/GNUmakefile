
include ../../../GDALmake.opt

OBJ =	ogrnasdriver.o ogrnasdatasource.o ogrnaslayer.o \
	nashandler.o nasreader.o ogrnasrelationlayer.o \
	gml3ogrgeometry.o

CPPFLAGS :=	-I../gml -I.. -I../.. -DHAVE_XERCES=1 \
		$(GDAL_INCLUDE) $(XERCES_INCLUDE) $(CPPFLAGS)

# By default, XML validation is disabled.  Uncomment the following line to
# enable XML schema validation in the parser.
#CPPFLAGS +=  -DOGR_GML_VALIDATION=1

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_nas.h nasreaderp.h ../gml/gmlreader.h ../gml/gmlreaderp.h
