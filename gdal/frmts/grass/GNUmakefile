

include ../../GDALmake.opt

#OBJ	=	grassdataset.o

ifeq ($(GRASS_SETTING),libgrass)
OBJ	=	grassdataset.o
else
OBJ	=	grass57dataset.o
endif

CPPFLAGS	:=	$(GRASS_INCLUDE) $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ)

clean:
	rm -f *.o $(O_OBJ) $(OBJ)
	rm -f *~

install-obj:	$(O_OBJ)

dist:
	cp -r pkg gdal-grass-$(GDAL_VER)
	rm -rf gdal-grass-$(GDAL_VER)/.svn
	cp grass57dataset.cpp gdal-grass-$(GDAL_VER)
	cp ../../ogr/ogrsf_frmts/grass/*.cpp gdal-grass-$(GDAL_VER)
	cp ../../ogr/ogrsf_frmts/grass/*.h gdal-grass-$(GDAL_VER)
	tar czvf gdal-grass-$(GDAL_VER).tar.gz ./gdal-grass-$(GDAL_VER)
	rm -rf gdal-grass-$(GDAL_VER)

