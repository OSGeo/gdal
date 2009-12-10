
include ../../GDALmake.opt

OBJ	=	pcidskdataset.o pcidsktiledrasterband.o \
		\
		pcidskdataset2.o vsi_pcidsk_io.o

CPPFLAGS	:=	$(GDAL_INCLUDE) -I../raw $(CPPFLAGS) $(PCIDSK_INC) 

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):	../raw/rawdataset.h gdal_pcidsk.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
