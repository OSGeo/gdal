include  ../GDALmake.opt

completion:
	PATH=$(GDAL_ROOT)/swig/python/scripts:$(GDAL_ROOT)/apps:$(PATH) python completionFinder.py gdal-bash-completion.sh

install:
	if test "x`pkg-config --version 2>/dev/null`" != "x" -a "x`pkg-config --variable=compatdir bash-completion`" != "x"; then \
		$(INSTALL_DIR) $(DESTDIR)${prefix}`pkg-config --variable=compatdir bash-completion` ; \
		cp gdal-bash-completion.sh $(DESTDIR)${prefix}`pkg-config --variable=compatdir bash-completion`; \
	fi
