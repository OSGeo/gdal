
include ../../GDALmake.opt

OBJ	=	netcdfdataset.o \
		netcdflayer.o \
		netcdfwriterconfig.o \
		gmtdataset.o \
		netcdfsg.o \
		netcdflayersg.o \
		netcdfsgwriterutil.o \
		netcdfmultidim.o \
		netcdfvirtual.o \
		netcdf_sentinel3_sral_mwr.o

XTRA_OPT =
ifeq ($(NETCDF_HAS_NC4),yes)
XTRA_OPT := -DNETCDF_HAS_NC4  $(XTRA_OPT)
endif
ifeq ($(NETCDF_HAS_HDF4),yes)
XTRA_OPT := -DNETCDF_HAS_HDF4 $(XTRA_OPT)
endif
ifeq ($(HAVE_HDF4),yes)
XTRA_OPT := -DHAVE_HDF4 $(XTRA_OPT)
endif
ifeq ($(HAVE_HDF5),yes)
XTRA_OPT := -DHAVE_HDF5 $(XTRA_OPT)
endif

CPPFLAGS	:=	 $(CPPFLAGS) $(XTRA_OPT)

ifeq ($(NETCDF_MEM),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DHAVE_NETCDF_MEM
endif

ifeq ($(ENABLE_UFFD),yes)
ifeq ($(NETCDF_MEM),yes)
CPPFLAGS	:=	$(CPPFLAGS) -DENABLE_UFFD
endif
endif

$(O_OBJ):	netcdfdataset.h

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
