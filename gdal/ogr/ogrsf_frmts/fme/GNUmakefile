

include ../../../GDALmake.opt

OBJ	=	ogrfmecacheindex.o ogrfmedatasource.o ogrfmedriver.o \
		ogrfmelayer.o ogrfmelayercached.o ogrfmelayerdb.o \
		fme2ogr_utils.o

CPPFLAGS	+=	-I.. -I../.. $(GDAL_INCLUDE) $(FME_INCLUDE) \
		-DSUPPORT_INDIRECT_FMEDLL -DSUPPORT_PERSISTENT_CACHE

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
