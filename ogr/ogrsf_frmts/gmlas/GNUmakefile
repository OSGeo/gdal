
include ../../../GDALmake.opt

OBJ =	ogrgmlasdriver.o ogrgmlasdatasource.o ogrgmlaslayer.o \
        ogrgmlasreader.o ogrgmlasschemaanalyzer.o ogrgmlasfeatureclass.o \
        ogrgmlasxsdcache.o ogrgmlasconf.o ogrgmlasxpatchmatcher.o \
        ogrgmlasxlinkresolver.o ogrgmlaswriter.o ogrgmlasutils.o

CPPFLAGS :=	-iquote ../mem -iquote ../geojson $(JSON_INCLUDE) -iquote .. -iquote ../.. -iquote ../pgdump -DHAVE_XERCES=1 \
		 $(XERCES_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_gmlas.h ogr_gmlas_consts.h
