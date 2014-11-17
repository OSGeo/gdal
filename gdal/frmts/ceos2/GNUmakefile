

OBJ	=	sar_ceosdataset.o \
		ceosrecipe.o ceossar.o ceos.o link.o

include ../../GDALmake.opt


CPPFLAGS	:=	-I../raw  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):	../raw/rawdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
