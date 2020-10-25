

include ../../GDALmake.opt

OBJ	=	grass.o

CPPFLAGS	:=	 -DGRASS_GISBASE=\"$(GRASS_GISBASE)\" $(GRASS_INCLUDE) $(PROJ_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -f *.o $(O_OBJ) $(OBJ)
	rm -f *~

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

dist:
	cp -r pkg gdal-grass-$(GDAL_VER)
	cp grass.cpp gdal-grass-$(GDAL_VER)
	cp ../../ogr/ogrsf_frmts/grass/*.cpp gdal-grass-$(GDAL_VER)
	cp ../../ogr/ogrsf_frmts/grass/*.h gdal-grass-$(GDAL_VER)
	tar czvf gdal-grass-$(GDAL_VER).tar.gz ./gdal-grass-$(GDAL_VER)
	rm -rf gdal-grass-$(GDAL_VER)

