
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ntffilereader.o ntfrecord.o ogrntfdatasource.o \
		ogrntfdriver.o ogrntflayer.o ntf_estlayers.o \
		ogrntffeatureclasslayer.o ntf_generic.o ntf_raster.o \
		ntf_codelist.o ntfstroke.o

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port $(CFLAGS)

LIBS	:=	../../ogr.a $(CPL_LIB) $(LIBS) 


default:	$(O_OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o

ntftest:	ntftest.o $(OBJ)
	$(CXX) $(LNK_FLAGS) ntftest.o $(LIBS) -o ntftest


ntfdump:	ntfdump.o $(OBJ)
	$(CXX) $(LNK_FLAGS) ntfdump.o $(LIBS) -o ntfdump


