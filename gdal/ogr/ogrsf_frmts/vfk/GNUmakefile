include ../../../GDALmake.opt

CORE_OBJ = vfkreader.o vfkdatablock.o vfkpropertydefn.o vfkfeature.o vfkproperty.o

OGR_OBJ = ogrvfkdriver.o ogrvfkdatasource.o ogrvfklayer.o

OBJ = $(CORE_OBJ) $(OGR_OBJ)

CPPFLAGS := -I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default: $(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ): ogr_vfk.h vfkreader.h vfkreaderp.h
