GDAL_ROOT	=	../..

include $(GDAL_ROOT)/GDALmake.opt

OBJ	=	ogdidataset.o

ALL_C_FLAGS	=	-I$(TOPDIR)/include $(GDAL_INCLUDE) $(CFLAGS)



default:	$(OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CC) -c $(ALL_C_FLAGS) $*.cpp -o $*.o
