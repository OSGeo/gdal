
include ../../GDALmake.opt

OBJ	=	pdsdataset.o isis2dataset.o isis3dataset.o vicardataset.o nasakeywordhandler.o vicarkeywordhandler.o

CPPFLAGS	:=	-I../raw  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

$(O_OBJ):       nasakeywordhandler.h vicarkeywordhandler.h ../raw/rawdataset.h

clean:
	rm -f *.o $(O_OBJ)

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

