include ../../GDALmake.opt

OBJ	=	MRRDataset.o MRRRasterBand.o MRRSDKImpl.o

LDFLAGS += -ldl -shared

CPPFLAGS	:=	-DFRMT_mrr -std=c++11 $(CPPFLAGS)

PLUGIN_SO =	gdal_MRR.so

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)
	rm -f *.lo $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

plugin: $(PLUGIN_SO)
	
#$(PLUGIN_SO):	$(OBJ:.o=.lo)
#	$(LD) $(LDFLAGS) ../../$(LIBGDAL) -o $@ $(OBJ:.o=.lo) \
#	    -rpath $(INST_LIB) \
#        -no-undefined -export-dynamic 

$(PLUGIN_SO):   $(OBJ)
		$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS) $(EXTRA_LIBS) \
			-o $(PLUGIN_SO)

