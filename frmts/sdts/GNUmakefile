
SHPDIR		=	/home/warmerda/shapelib-1.2.5
ISO8211DIR	=	../iso8211
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	sdtsiref.o sdtscatd.o sdtslinereader.o sdtslib.o \
		sdtspointreader.o sdtsattrreader.o sdtstransfer.o \
		sdtspolygonreader.o

ALL_C_FLAGS =	-I$(ISO8211DIR) $(GDAL_INCLUDE) $(CFLAGS)
ALL_CXX_FLAGS =	-I$(ISO8211DIR) $(GDAL_INCLUDE) $(CXXFLAGS)
SDTSLIB	=	libsdts_al.a
LIBS	:=	$(SDTSLIB) $(ISO8211DIR)/libiso8211.a $(CPL_LIB) $(LIBS)



default:	$(SDTSLIB)

all:	sdts2ascii sdts2shp

clean:
	rm -f *.o sdts2ascii 8211dump

%.o:	%.cpp
	$(CXX) -c $(ALL_CXX_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o

$(SDTSLIB):	$(OBJ)
	ar r $(SDTSLIB) $?


sdts2ascii:	sdts2ascii.o $(SDTSLIB)
	$(CXX) sdts2ascii.o $(LIBS) -o sdts2ascii

sdtsinfo:	sdtsinfo.o $(SDTSLIB)
	$(CXX) sdtsinfo.o $(LIBS) -o sdtsinfo

sdts2shp:	sdts2shp.cpp $(SDTSLIB)
	$(CXX) $(ALL_C_FLAGS) -I$(SHPDIR) sdts2shp.cpp \
		$(SHPDIR)/shpopen.o $(SHPDIR)/dbfopen.o \
		$(LIBS) -o sdts2shp

install-obj:	$(OBJ)
	cp $(OBJ) ../o

