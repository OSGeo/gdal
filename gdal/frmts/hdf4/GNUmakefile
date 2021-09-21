
include ../../GDALmake.opt

OBJ	=	hdf4dataset.o hdf4imagedataset.o hdf4multidim.o

SUBLIBS 	= lib-hdfeos

CPPFLAGS	:=	-iquote hdf-eos $(HDF4_INCLUDE) $(HDF4_FLAGS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)
	(cd hdf-eos; $(MAKE) clean)

lib-hdfeos:
	(cd hdf-eos; $(MAKE) install-obj)

install-obj:	$(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))

EOS_SO          = ../o/EHapi.o ../o/GDapi.o ../o/SWapi.o ../o/gctp_wrap.o

PLUGIN_SO       = gdal_HDF4.$(SO_EXT)

plugin: $(PLUGIN_SO)

$(PLUGIN_SO):   $(OBJ)
	$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(EOS_SO) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
