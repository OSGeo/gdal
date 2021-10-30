
OBJ	=	zarrdriver.o zarr_array.o zarr_attribute.o zarr_group.o

include ../../GDALmake.opt

XTRA_OPT=
ifeq ($(HAVE_BLOSC),yes)
XTRA_OPT := $(XTRA_OPT) -DHAVE_BLOSC
endif

CPPFLAGS :=  -iquote ../mem -iquote ../netcdf $(CPPFLAGS) $(XTRA_OPT)

CXXFLAGS	:=	$(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

$(O_OBJ): zarr.h

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
