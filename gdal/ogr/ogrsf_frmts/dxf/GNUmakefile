
include ../../../GDALmake.opt

OBJ	=	ogrdxfdriver.o ogrdxfdatasource.o ogrdxflayer.o \
		ogrdxfreader.o ogrdxf_blockmap.o ogrdxf_dimension.o \
		ogrdxfwriterds.o ogrdxfwriterlayer.o intronurbs.o \
		ogrdxf_polyline_smooth.o ogrdxfblockslayer.o \
		ogrdxfblockswriterlayer.o ogrdxf_hatch.o \
		ogr_autocad_services.o ogrdxf_feature.o \
		ogrdxf_leader.o ogrdxf_ocstransformer.o

CPPFLAGS	:=	-iquote .. -iquote ../..  $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_dxf.h

