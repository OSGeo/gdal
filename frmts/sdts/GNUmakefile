
SHPDIR		=	/home/warmerda/shapelib-1.2.5
ISO8211DIR	=	../iso8211
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	sdtsiref.o sdtscatd.o sdtslinereader.o sdtslib.o \
		sdtspointreader.o sdtsattrreader.o

ALL_C_FLAGS	=	-I$(ISO8211DIR) $(GDAL_INCLUDE) $(CFLAGS)
LIBS	:=	$(ISO8211DIR)/libiso8211.a $(LIBS)



default:	$(OBJ)

all:	sdts2ascii sdts2shp

clean:
	rm -f *.o sdts2ascii 8211dump

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o


sdts2ascii:	sdts2ascii.o $(OBJ)
	$(CXX) sdts2ascii.o $(OBJ) $(GDAL_ROOT)/port/*.o $(LIBS) -o sdts2ascii

sdts2shp:	sdts2shp.cpp $(OBJ)
	$(CXX) $(ALL_C_FLAGS) -I$(SHPDIR) sdts2shp.cpp $(OBJ) \
		$(SHPDIR)/shpopen.o $(SHPDIR)/dbfopen.o \
		$(GDAL_ROOT)/port/*.o $(LIBS) -o sdts2shp

install-obj:	$(OBJ)
	cp $(OBJ) ../o

