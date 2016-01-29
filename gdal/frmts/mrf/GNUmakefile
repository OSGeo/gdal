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

# Modified for MRF plugin

include ../../GDALmake.opt

PLUGIN_PATH	=	$(prefix)/lib/gdalplugins/$(GDAL_VERSION_MAJOR).$(GDAL_VERSION_MINOR)
FILES	=	marfa_dataset mrf_band JPEG_band PNG_band Raw_band Tif_band mrf_util mrf_overview
OBJ 	=	$(addsuffix .o, $(FILES))
LO_O_OBJ	=	$(addsuffix .lo,$(basename $(O_OBJ)))
DEPENDS	= marfa.h

# JPEG12_ENABLED is set only with the internal jpeg library
ifeq ($(JPEG12_ENABLED),yes)
FILES	:= $(FILES) JPEG12_band
CPPFLAGS := -DJPEG12_SUPPORTED
GDAL_INCLUDE := -I../jpeg/libjpeg $(GDAL_INCLUDE)
endif

ifneq ("$(wildcard LERC_band.cpp)","")
FILES	:= $(FILES) LERC_band BitMask BitStuffer CntZImage Lerc2 BitStuffer2 BitMask2 RLE Huffman
DEPENDS  := $(DEPENDS) BitMask.h BitStuffer.h CntZImage.h Defines.h Image.h TImage.hpp Lerc2.h BitStuffer2.h BitMask2.h RLE.h Huffman.h
CPPFLAGS := -DLERC
endif

CPPFLAGS	:= -fPIC $(GDAL_INCLUDE) $(CPPFLAGS)

default:	$(OBJ:.o=.$(OBJ_EXT))

clean:
	rm -rf *.o *.lo .libs/$(OBJ) .libs $(O_OBJ) $(LO_O_OBJ) gdal_mrf.so.1

install-obj:	$(O_OBJ:.o=.$(OBJ_EXT))

$(OBJ) $(O_OBJ):    $(DEPENDS)

plugin: $(OBJ)
	g++ -shared -W1,-soname,gdal_mrf.so.1 $(OBJ) -o gdal_mrf.so.1

iplugin: gdal_mrf.so.1 plugin
	mkdir -p $(PLUGIN_PATH)
	cp $< $(PLUGIN_PATH)/gdal_mrf.so
