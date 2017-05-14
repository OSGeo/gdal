
include ../GDALmake.opt

OBJ	=	gdal_fuzzer.o ogr_fuzzer.o shape_fuzzer.o wkb_import_fuzzer.o wkt_import_fuzzer.o

CPPFLAGS	:=	$(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	$(RM) *.o $(O_OBJ)

