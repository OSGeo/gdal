
#SDTSHOME	=	/home/warmerda/pkg/libsdts++-0.5.3
SDTSHOME	=	/home/warmerda/pkg/sdts++-1.0pre
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	sdtsiref.o sdtscatd.o sdtslinereader.o sdtsattrreader.o \
		sdtslib.o

ALL_C_FLAGS	=	-I$(SDTSHOME) -I$(SDTSHOME)/sdts++ \
			$(GDAL_INCLUDE) $(CFLAGS)
LIBS	:=	$(SDTSHOME)/sdts++/libsdts++.a $(LIBS)



default:	$(OBJ)

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

8211dump:	8211dump.o
	$(CXX) 8211dump.o $(GDAL_ROOT)/port/*.o $(LIBS) -o 8211dump

install-obj:	$(OBJ)
	cp $(OBJ) ../o

