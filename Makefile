PROJ4 = proj4
GDAL = gdal
EMMAKE ?= emmake
EMCC ?= emcc
EMCONFIGURE ?= emconfigure
EMCONFIGURE_JS ?= 0
GDAL_EMCC_CFLAGS := -msse -Oz
PROJ_EMCC_CFLAGS := -msse -Oz
EXPORTED_FUNCTIONS = "[\
  '_GDALAllRegister',\
  '_GDALOpen',\
  '_GDALGetRasterXSize',\
  '_GDALGetRasterYSize',\
  '_GDALGetRasterCount',\
  '_GDALGetProjectionRef',\
  '_GDALGetGeoTransform'\
]"

export EMCONFIGURE_JS

include gdal-configure.opt

.PHONY: clean

########
# GDAL #
########
gdal: gdal.js

gdal.js: gdal-lib
	EMCC_CFLAGS="$(GDAL_EMCC_CFLAGS)" $(EMCC) $(GDAL)/libgdal.a -o gdal.js -s EXPORTED_FUNCTIONS=$(EXPORTED_FUNCTIONS)

gdal-lib: $(GDAL)/libgdal.a

$(GDAL)/libgdal.a: $(GDAL)/config.status proj4
	cd $(GDAL) && EMCC_CFLAGS="$(GDAL_EMCC_CFLAGS)" $(EMMAKE) make lib-target

# TODO: Pass the configure params more elegantly so that this uses the
# EMCONFIGURE variable
$(GDAL)/config.status: proj4-native $(GDAL)/configure
	cd $(GDAL) && emconfigure ./configure $(GDAL_CONFIG_OPTIONS)

##########
# PROJ.4 #
##########
# The -X parameter cleans only ignored files but not manually added ones
reset-proj4:
	cd $(PROJ4) && git clean -X --force

proj4: reset-proj4
	cd $(PROJ4) && ./autogen.sh
	cd $(PROJ4) && $(EMCONFIGURE) ./configure
	cd $(PROJ4) && EMCC_CFLAGS="$(PROJ_EMCC_CFLAGS)" $(EMMAKE) make

# We need to make proj4-native a separate prerequisite even though it's nearly
# the same thing because make will only run each prerequisite once per
# invocation, and we need to build PROJ.4 twice, once natively to fake out the
# GDAL build script, and once "for real" via emscripten.  For the same reason,
# we also can't target any of the file outputs of the PROJ.4 build process
# because the files are exactly the same no matter whether the build is native
# or via emscripten, so if we target individual files, they'll only get built
# once, not twice as we need.
# TODO: Make this cleaner.
reset-proj4-native:
	cd $(PROJ4) && git clean -X --force

proj4-native: reset-proj4-native
	cd $(PROJ4) && ./autogen.sh
	cd $(PROJ4) && ./configure
	cd $(PROJ4) && make

clean:
	cd $(GDAL) && make clean
	cd $(PROJ4) && make clean
	rm -f gdal.js gdal.js.mem
