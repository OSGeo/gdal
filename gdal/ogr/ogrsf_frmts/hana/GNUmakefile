# $Id$
#
# Makefile to build OGR SAP HANA driver
#

include ../../../GDALmake.opt

OBJ	=	ogrhanadriver.o ogrhanadatasource.o ogrhanautils.o \
      ogrhanatablelayer.o ogrhanalayer.o ogrhanaresultlayer.o \
      ogrhanafeaturereader.o ogrhanafeaturewriter.o

CPPFLAGS	:=	-I.. -I../..  $(ODBCCPP_INC) -DODBC_STATIC $(CPPFLAGS)

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogr_hana.h ogrhanautils.h ogrhanafeaturereader.h ogrhanafeaturewriter.h

PLUGIN_SO	= ogr_HANA.$(SO_EXT)

plugin:		$(PLUGIN_SO)

$(PLUGIN_SO):	$(OBJ)
		$(LD_SHARED) $(LNK_FLAGS) $(OBJ) $(CONFIG_LIBS_INS) $(LIBS) \
		-o $(PLUGIN_SO)
