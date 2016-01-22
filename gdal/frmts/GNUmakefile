

include ../GDALmake.opt

OBJ	=	o/gdalallregister.o

FRMT_FLAGS	=	$(foreach FRMT, $(GDAL_FORMATS), -DFRMT_$(FRMT))

%-install-obj:
	$(MAKE) -C $* install-obj

default:	$(OBJ:.o=.$(OBJ_EXT)) $(foreach d,$(GDAL_FORMATS),$(d)-install-obj)

clean:	$(foreach d,$(GDAL_FORMATS),$(d)-clean)
	rm -f *.o o/*.o o/*.a
	$(RM) o/*.lo

o/gdalallregister.$(OBJ_EXT):	gdalallregister.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CXXFLAGS) $(FRMT_FLAGS) \
		-DGDAL_FORMATS="$(GDAL_FORMATS)" \
		gdalallregister.cpp -o o/gdalallregister.$(OBJ_EXT)

# We might want to add dynamically generated drivers here eventually.
install:
	$(MAKE) -C vrt install
	$(MAKE) -C mem install
	$(MAKE) -C raw install
