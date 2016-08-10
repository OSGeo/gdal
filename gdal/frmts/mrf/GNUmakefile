# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include ../../GDALmake.opt

PLUGIN_PATH	=	$(prefix)/lib/gdalplugins/$(GDAL_VERSION_MAJOR).$(GDAL_VERSION_MINOR)
FILES	=	marfa_dataset mrf_band JPEG_band PNG_band JPNG_band Raw_band Tif_band mrf_util mrf_overview
OBJ 	=	$(addsuffix .o, $(FILES))
LO_O_OBJ	=	$(addsuffix .lo,$(basename $(O_OBJ)))
DEPENDS	= marfa.h

# For GDAL < 2.1
CPPFLAGS        :=      $(GDAL_INCLUDE) $(CPPFLAGS)

# JPEG12_ENABLED is set only with the internal jpeg library
ifeq ($(JPEG12_ENABLED),yes)
FILES	:= $(FILES) JPEG12_band
XTRA_OPT := $(XTRA_OPT) -DJPEG12_SUPPORTED
endif

ifeq ($(JPEG_SETTING),internal) 
XTRA_OPT        :=      $(XTRA_OPT) -I../jpeg/libjpeg
endif

ifeq ($(PNG_SETTING),internal) 
XTRA_OPT        :=      $(XTRA_OPT) -I../png/libpng -DINTERNAL_PNG
endif

ifeq ($(LIBZ_SETTING),internal) 
XTRA_OPT        :=      $(XTRA_OPT) -I../zlib
endif

ifneq ("$(wildcard libLERC)","")
LIBLERC	= libLERC
OBJ	:=	$(OBJ) LERC_band.o
XTRA_OPT	:=	$(XTRA_OPT) -DLERC -I $(LIBLERC)
SUBLIBS := libLERC
endif

CPPFLAGS        :=  $(XTRA_OPT)  $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT)) $(SUBLIBS)

clean:
	rm -rf *.o *.lo .libs/$(OBJ) .libs $(O_OBJ) $(LO_O_OBJ) gdal_mrf.so.1
	cd $(LIBLERC); $(MAKE) clean

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))
	cd $(LIBLERC); $(MAKE) install-obj

$(OBJ) $(O_OBJ):    $(DEPENDS)

plugin: $(OBJ)
	g++ -shared -Wl,-soname,gdal_mrf.so.1 $(OBJ) -o gdal_mrf.so.1

iplugin: gdal_mrf.so.1 plugin
	mkdir -p $(PLUGIN_PATH)
	cp $< $(PLUGIN_PATH)/gdal_mrf.so
