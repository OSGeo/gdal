

include ../../../GDALmake.opt

OBJ	=	ogrdxfdriver.o ogrdxfdatasource.o ogrdxflayer.o \
		ogrdxf_diskio.o ogrdxf_blockmap.o ogrdxf_dimension.o \
		ogrdxfwriterds.o ogrdxfwriterlayer.o intronurbs.o \
		ogrdxf_polyline_smooth.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS) -DDEBUG

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_dxf.h

