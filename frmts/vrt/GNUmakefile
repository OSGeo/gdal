

include ../../GDALmake.opt

OBJ	=	vrtdataset.o vrtrasterband.o vrtdriver.o vrtsources.o \
		vrtfilters.o vrtsourcedrasterband.o vrtrawrasterband.o \
		vrtwarped.o vrtderivedrasterband.o

CPPFLAGS	:=	$(GDAL_INCLUDE) $(CPPFLAGS) -I../raw

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ)

$(OBJ) $(O_OBJ):	vrtdataset.h

install:
	$(INSTALL_DATA) vrtdataset.h $(DESTDIR)$(INST_INCLUDE)
