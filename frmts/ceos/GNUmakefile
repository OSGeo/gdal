
GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ceosopen.o ceosdataset.o

ALL_C_FLAGS	=	$(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.cpp -o $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) -I$(GDAL_ROOT)/core -I$(GDAL_ROOT)/port \
		$*.c -o $*.o

ceostest:	ceostest.o ceosopen.o
	$(CXX) ceostest.o ceosopen.o $(GDAL_ROOT)/port/*.o $(LIBS) -o ceostest

install-obj:	$(OBJ)
	cp $(OBJ) ../o

