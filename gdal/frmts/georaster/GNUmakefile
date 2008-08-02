include ../../GDALmake.opt

OBJ		=	geor_driver.o \
			geor_dataset.o \
			geor_band.o \
			orcl_oci.o \
			orcl_geor.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(OCI_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)
	rm -f *.lo
	rm -f *.so   

install-obj:	$(O_OBJ)

plugin:	gdal_GEOR.so

gdal_GEOR.so:	$(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) -o gdal_GEOR.so
