include ../../GDALmake.opt

OBJ		=	georaster_driver.o \
			georaster_dataset.o \
			georaster_rasterband.o \
			oci_wrapper.o \
			georaster_wrapper.o

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
