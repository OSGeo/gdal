
GDAL_ROOT = ../..

include ../../GDALmake.opt

BINDING = csharp
include ../SWIGmake.base

clean:
	-rm -rf build

veryclean: clean
	-rm -f ${WRAPPERS}

generate: ${WRAPPERS}

build: generate
	@echo "What are you doing?  C# modules needs to be built with msvc"

install: generate
	@echo "What are you doing?  C# modules needs to be built with msvc"
