

include ../../GDALmake.opt

OBJ	=	ersdataset.o ershdrnode.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I../raw $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

plugin: gdal_ERS.so

gdal_ERS.so:	$(OBJ)
	$(LD_SHARED) $(OBJ) $(GDAL_LIBS) $(LIBS) -o gdal_ERS.so

