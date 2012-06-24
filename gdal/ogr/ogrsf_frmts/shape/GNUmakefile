

include ../../../GDALmake.opt

OBJ	=	shape2ogr.o shpopen.o dbfopen.o shptree.o sbnsearch.o shp_vsi.o \
		ogrshapedriver.o ogrshapedatasource.o ogrshapelayer.o

CPPFLAGS :=	-DSAOffset=vsi_l_offset -DUSE_CPL \
		-I.. -I../.. $(GDAL_INCLUDE) $(CPPFLAGS) 

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	ogrshape.h shapefil.h

clean:
	rm -f *.o $(O_OBJ)

import:
	@if test ! -d ~/shapelib ; then \
	  echo reimport requires shapelib checked out as ~/shapelib ; \
	  exit 1; \
	fi

	copymatch.sh ~/shapelib *.c *.h

	@echo
	@echo 'Now do something like:'
	@echo '% svn commit -m "updated to shapelib CVS"'
	@echo

