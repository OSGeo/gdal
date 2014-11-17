

include ../../GDALmake.opt

OBJ	=	vrtdataset.o vrtrasterband.o vrtdriver.o vrtsources.o \
		vrtfilters.o vrtsourcedrasterband.o vrtrawrasterband.o \
		vrtwarped.o vrtderivedrasterband.o

CPPFLAGS	:=	-I../raw  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	vrtdataset.h ../../alg/gdalwarper.h ../raw/rawdataset.h ../../gcore/gdal_proxy.h

install:
	$(INSTALL_DATA) vrtdataset.h $(DESTDIR)$(INST_INCLUDE)
	$(INSTALL_DATA) gdal_vrt.h $(DESTDIR)$(INST_INCLUDE)
