
GDAL_ROOT	=	../../..

include $(GDAL_ROOT)/GDALmake.opt

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

ALL_C_FLAGS	=	-I.. -I../.. -I$(GDAL_ROOT)/port $(CFLAGS) -DOGR

LIBS	:=	../ogrsf_frmts.a ../../ogr.a $(CPL_LIB) $(LIBS) 


default:	$(O_OBJ)

clean:
	rm -f *.o

%.o:	%.cpp
	$(CXX) -c $(ALL_C_FLAGS) $*.cpp -o $*.o
	ar r ../ogrsf_frmts.a $*.o

%.o:	%.c
	$(CC) -c $(ALL_C_FLAGS) $*.c -o $*.o
	ar r ../ogrsf_frmts.a $*.o

