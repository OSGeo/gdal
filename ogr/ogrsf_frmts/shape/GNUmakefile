

include ../../../GDALmake.opt

OBJ	=	shape2ogr.o shpopen_wrapper.o dbfopen_wrapper.o shptree_wrapper.o sbnsearch_wrapper.o shp_vsi.o \
		ogrshapedriver.o ogrshapedatasource.o ogrshapelayer.o

CPPFLAGS :=	-DSAOffset=vsi_l_offset -DUSE_CPL \
		-iquote .. -iquote ../.. -iquote ../generic  $(CPPFLAGS)

ifeq ($(RENAME_INTERNAL_SHAPELIB_SYMBOLS),yes)
CPPFLAGS 	:=	$(CPPFLAGS) -DRENAME_INTERNAL_SHAPELIB_SYMBOLS -DSHPAPI_CALL=
endif

CXXFLAGS        :=      $(WARN_EFFCPLUSPLUS) $(WARN_OLD_STYLE_CAST) $(CXXFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):	ogrshape.h shapefil.h shpopen.c dbfopen.c shptree.c sbnsearch.c

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

