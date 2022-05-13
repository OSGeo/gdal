include  ../GDALmake.opt

PROGRAMS := gdal2tiles.py gdal2xyz.py gdaladdo gdalbuildvrt gdal_calc.py \
            gdalchksum.py gdalcompare.py gdal-config gdal_contour gdaldem \
            gdal_edit.py gdalenhance gdal_fillnodata.py gdal_grid \
            gdalident.py gdalimport.py gdallocationinfo gdalmanage \
            gdal_merge.py gdalmove.py gdal_polygonize.py gdal_proximity.py \
            gdal_rasterize gdal_retile.py gdal_sieve.py \
            gdalsrsinfo gdaltindex gdaltransform gdal_translate gdalwarp \
            ogr2ogr ogrinfo ogrlineref ogrmerge.py ogrtindex gdal_viewshed gdal_create

completion:
	PATH=$(GDAL_ROOT)/swig/python/gdal-utils/scripts:$(GDAL_ROOT)/apps:$(PATH) python completionFinder.py gdal-bash-completion.sh

install:
	$(INSTALL_DIR) $(DESTDIR)$(INST_BASH_COMPLETION) ; \
	$(INSTALL_DATA) gdal-bash-completion.sh $(DESTDIR)$(INST_BASH_COMPLETION)/gdalinfo; \
	for p in $(PROGRAMS); do \
		ln -rs $(DESTDIR)$(INST_BASH_COMPLETION)/gdalinfo $(DESTDIR)$(INST_BASH_COMPLETION)/$$p; \
	done
