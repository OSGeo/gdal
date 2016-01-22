

include ../../../GDALmake.opt

CORE_OBJ = ogrlibkmlgeometry.o \
           ogrlibkmlfield.o \
           ogrlibkmlfeature.o \
           ogrlibkmlfeaturestyle.o \
           ogrlibkmlstyle.o

OGR_OBJ =	ogrlibkmldriver.o ogrlibkmldatasource.o ogrlibkmllayer.o

OBJ =	$(CORE_OBJ) $(OGR_OBJ)

CPPFLAGS := -I. -I.. -I../.. -I$(GDAL_ROOT)/port -I$(GDAL_ROOT)/gcore -I$(GDAL_ROOT)/alg \
	-I$(GDAL_ROOT)/ogr -I$(GDAL_ROOT)/ogrsf_frmts $(LIBKML_INCLUDE) $(CPPFLAGS)

#CFLAGS	:=	$(filter-out -Wall,$(CFLAGS))

default:	$(O_OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ)

$(O_OBJ):	ogrlibkmlgeometry.h \
            ogrlibkmlfield.h \
            ogrlibkmlfeature.h \
            ogrlibkmlfeaturestyle.h \
            ogrlibkmlstyle.h \
            ogr_libkml.h
          
