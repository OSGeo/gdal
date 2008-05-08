

include ../../../GDALmake.opt

OBJ	=	ntffilereader.o ntfrecord.o ogrntfdatasource.o \
		ogrntfdriver.o ogrntflayer.o ntf_estlayers.o \
		ogrntffeatureclasslayer.o ntf_generic.o ntf_raster.o \
		ntf_codelist.o ntfstroke.o

CPPFLAGS	:=	-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(O_OBJ)

clean:
	rm -f *.o $(O_OBJ)

ntfdump$(EXE):	ntfdump.o
	$(LD) $(LDFLAGS) ntfdump.o $(CONFIG_LIBS) -o ntfdump$(EXE)


