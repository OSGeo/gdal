

include ../../GDALmake.opt

OBJ	=	rawdataset.o ehdrdataset.o pauxdataset.o doq1dataset.o \
		doq2dataset.o mffdataset.o hkvdataset.o pnmdataset.o \
		fujibasdataset.o envidataset.o gscdataset.o fastdataset.o \
		atlsci_spheroid.o btdataset.o landataset.o cpgdataset.o \
		idadataset.o ndfdataset.o dipxdataset.o genbindataset.o \
		lcpdataset.o eirdataset.o gtxdataset.o loslasdataset.o \
		ntv2dataset.o ace2dataset.o snodasdataset.o ctable2dataset.o \
		krodataset.o



default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	rawdataset.h

install:
	$(INSTALL_DATA) rawdataset.h $(DESTDIR)$(INST_INCLUDE)
