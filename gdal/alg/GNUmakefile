
include ../GDALmake.opt

OBJ	=	gdalmediancut.o gdaldither.o gdal_crs.o gdaltransformer.o \
		gdalsimplewarp.o gdalwarper.o gdalwarpkernel.o \
		gdalwarpoperation.o gdalchecksum.o gdal_rpc.o gdal_tps.o \
		thinplatespline.o llrasterize.o gdalrasterize.o gdalgeoloc.o

ifeq ($(OGR_ENABLED),yes)
OBJ += contour.o
endif


CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	$(RM) *.o

docs:	
	(cd ..; $(MAKE) docs)

install:
	for f in *.h ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done

lib:	$(OBJ)
	(cd .. ; $(MAKE) force-lib)
