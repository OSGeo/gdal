include ../GDALmake.opt

INST_H_FILES = \
	cpl_config.h \
	gdal_version.h

clean:
	$(RM) -f gdal_version.h

install:
	for f in $(INST_H_FILES) ; do $(INSTALL_DATA) $$f $(DESTDIR)$(INST_INCLUDE) ; done
