
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogrfmecacheindex.o ogrfmedatasource.o ogrfmedriver.o \
		ogrfmelayer.o ogrfmelayercached.o ogrfmelayerdb.o \
		fme2ogr_utils.o

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port \
		$(FME_INCLUDE) $(CFLAGS) \
		-DSUPPORT_INDIRECT_FMEDLL \
		-DSUPPORT_PERSISTENT_CACHE

default:	$(O_OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o

