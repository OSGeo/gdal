
GDAL_ROOT	=	../..

OBJ	=	sar_ceosdataset.o \
		ceosrecipe.o ceossar.o ceos.o link.o

include $(GDAL_ROOT)/GDALmake.opt

XTRAOPTS =	-I../raw

default:	$(OBJ)

clean:
	rm -f *.o
 
%.o:    %.c
	$(CC) -c $(GDAL_INCLUDE) $(XTRAOPTS) $(CFLAGS) $*.c -o $*.o

%.o:    %.cpp     
	$(CXX) -c $(GDAL_INCLUDE) $(XTRAOPTS) $(CXXFLAGS) $*.cpp -o $*.o


install-obj:	$(foreach file,$(OBJ),../o/$(file))
