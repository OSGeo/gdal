
GDAL_ROOT	=	..

include ../GDALmake.opt

OBJ	=	o/gdalallregister.o

FRMT_FLAGS	=	$(foreach FRMT, $(GDAL_FORMATS), -DFRMT_$(FRMT))

default:	$(OBJ) $(foreach d,$(GDAL_FORMATS),$(d)-target-install-obj)

clean:	$(foreach d,$(GDAL_FORMATS),$(d)-clean)
	rm -f *.o o/*.o o/*.a

o/gdalallregister.o:	gdalallregister.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $(FRMT_FLAGS) \
		-DGDAL_FORMATS="$(GDAL_FORMATS)" \
		gdalallregister.cpp -o o/gdalallregister.o

# We might want to add dynamically generated drivers here eventually.
install:
