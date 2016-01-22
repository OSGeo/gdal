

include ../../../GDALmake.opt

OBJ	=	ntffilereader.o ntfrecord.o ogrntfdatasource.o \
		ogrntfdriver.o ogrntflayer.o ntf_estlayers.o \
		ogrntffeatureclasslayer.o ntf_generic.o ntf_raster.o \
		ntf_codelist.o ntfstroke.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

ntfdump$(EXE):	ntfdump.$(OBJ_EXT)
	$(LD) $(LDFLAGS) ntfdump.$(OBJ_EXT) $(CONFIG_LIBS) -o ntfdump$(EXE)


