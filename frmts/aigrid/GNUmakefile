
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

AIGOBJ  =	gridlib.o aigopen.o
OBJ	=	$(AIGOBJ)

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o aitest

%.o:	%.cpp
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o


aitest:	aitest.o $(AIGOBJ)
	$(CC) aitest.o $(AIGOBJ) $(GDAL_ROOT)/port/*.o $(LIBS) -o aitest


