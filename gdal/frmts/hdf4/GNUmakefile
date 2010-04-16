
include ../../GDALmake.opt

OBJ	=	hdf4dataset.o hdf4imagedataset.o

HDFEOS_OPTS	=	
SUBLIBS 	=

HDF4EOS_SETTING = internal
ifeq ($(HDF4EOS_SETTING),internal)
SUBLIBS	:= lib-hdfeos $(SUBLIBS) 
HDFEOS_OPTS :=	-Ihdf-eos $(HDFEOS_OPTS)
endif

CPPFLAGS	:=	-I../pds $(GDAL_INCLUDE) $(HDF4_INCLUDE) $(HDFEOS_OPTS) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) $(SUBLIBS)

clean:
	rm -f *.o $(O_OBJ)
	(cd hdf-eos; $(MAKE) clean)

lib-hdfeos:
	(cd hdf-eos; $(MAKE) install-obj)

install-obj:	$(SUBLIBS) $(O_OBJ:.o=.$(OBJ_EXT))
