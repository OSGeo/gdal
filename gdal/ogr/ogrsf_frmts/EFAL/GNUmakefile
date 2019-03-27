

include ../../../GDALmake.opt

OBJ	=	from_mitab.o \
		ogrefaldatasource.o \
		ogrefaldriver.o \
		ogrefallayer.o \
		ogrgeopackageutility.o \
		style_utility.o

CPPFLAGS	:=	-I. -I.. -I../.. -I../generic -I../gpkg  $(CPPFLAGS) -DOGR

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):       MIDefs.h EFAL.h EFALAPI.h EFALErrorIDs.h EFALLIB.h from_mitab.h OGREFAL.h ogrgeopackageutility.h

update:
	copymatch.sh ~/pkg/efal *.TXT
	copymatch.sh ~/pkg/efal/efal *.cpp *.h
