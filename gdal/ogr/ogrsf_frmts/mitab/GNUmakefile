

include ../../../GDALmake.opt

OBJ	=	mitab_rawbinblock.o mitab_mapheaderblock.o \
		mitab_mapindexblock.o mitab_mapobjectblock.o \
		mitab_mapcoordblock.o mitab_feature.o mitab_feature_mif.o \
		mitab_mapfile.o mitab_idfile.o mitab_datfile.o \
		mitab_tabfile.o mitab_miffile.o mitab_utils.o \
		mitab_imapinfofile.o mitab_middatafile.o mitab_bounds.o \
		mitab_maptoolblock.o mitab_tooldef.o mitab_coordsys.o \
		mitab_spatialref.o mitab_ogr_driver.o mitab_indfile.o \
		mitab_tabview.o mitab_ogr_datasource.o mitab_geometry.o \
		mitab_tabseamless.o

CPPFLAGS	:=	-I.. -I../..  $(CPPFLAGS) -DOGR \
			-DMITAB_USE_OFTDATETIME

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):       mitab.h mitab_priv.h mitab_ogr_driver.h

update:
	copymatch.sh ~/pkg/mitab *.TXT
	copymatch.sh ~/pkg/mitab/mitab *.cpp *.h
