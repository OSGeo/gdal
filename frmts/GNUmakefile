

include ../GDALmake.opt

OBJ	=	o/gdalallregister.o

FRMT_FLAGS	=	$(foreach FRMT, $(GDAL_FORMATS), -DFRMT_$(FRMT))

ifeq ($(GNM_ENABLED),yes)
   GDAL_INCLUDE += -iquote $(GDAL_ROOT)/gnm -iquote $(GDAL_ROOT)/gnm/gnm_frmts
   CXXFLAGS += -DGNM_ENABLED
endif

%-install-obj:
	$(MAKE) -C $* install-obj

default:	$(OBJ:.o=.$(OBJ_EXT)) $(foreach d,$(GDAL_FORMATS),$(d)-install-obj)

clean:	$(foreach d,$(GDAL_FORMATS),$(d)-clean)
	rm -f *.o o/*.o o/*.a
	$(RM) o/*.lo

o/gdalallregister.$(OBJ_EXT):	gdalallregister.cpp ../GDALmake.opt
	$(CXX) -c $(GDAL_INCLUDE) $(CPPFLAGS) $(CXXFLAGS) $(FRMT_FLAGS) \
		-DGDAL_FORMATS="$(GDAL_FORMATS)" \
		gdalallregister.cpp -o o/gdalallregister.$(OBJ_EXT)

# We might want to add dynamically generated drivers here eventually.
install:
	$(MAKE) -C vrt install
	$(MAKE) -C mem install
	$(MAKE) -C raw install
	$(INSTALL_DIR) $(DESTDIR)$(INST_LIB)/gdalplugins
	$(INSTALL_DATA) drivers.ini $(DESTDIR)$(INST_LIB)/gdalplugins
