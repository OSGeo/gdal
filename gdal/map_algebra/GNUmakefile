inc = /home/ajolma/usr/include

gdal = -L/home/ajolma/usr/lib -lgdal

gma = -L/home/ajolma/github/gdal/gdal/map_algebra -lgma

cpp_objects = gma_classes.o

c_objects = gma_c_api.o

cpp_headers = gdal_map_algebra_core.h gdal_map_algebra_classes.hpp gdal_map_algebra.hpp gma_classes.hpp gma_band.hpp gma_two_bands.hpp

c_headers = gdal_map_algebra_core.h gdal_map_algebra_types.h gdal_map_algebra.h

shared_library = libgma.so

programs = test demo histogram d8 test-c-api

swig_interface = gma.i

perl_module = gma.pm

all: $(programs) $(perl_module) perl

test: test.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc -o test -I $(inc) test.cpp $(gma) $(gdal) -g -lstdc++ -lm

demo: demo.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc -o demo -I $(inc) demo.cpp $(gma) $(gdal) -g -lstdc++ -lm

histogram: histogram.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc -o histogram -I $(inc) histogram.cpp $(gma) $(gdal) -g -lstdc++ -lm

d8: d8.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc -o d8 -I $(inc) d8.cpp $(gma) $(gdal) -g -lstdc++ -lm

test-c-api: test-c-api.c $(c_objects) $(cpp_objects)
	gcc -o test-c-api -I $(inc) test-c-api.c $(c_objects) $(gma) $(gdal) -g -lstdc++ -lm

gma_classes.o: gma_classes.cpp $(cpp_headers)
	gcc -c -Wall -Werror -fpic -g -I $(inc) gma_classes.cpp

$(shared_library): gma_classes.o
	gcc -shared -o $(shared_library) gma_classes.o

gma_c_api.o: gma_c_api.cpp $(c_headers)
	gcc -c -g -I $(inc) gma_c_api.cpp $(cpp_objects)

$(perl_module): $(swig_interface)
	swig -c++ -perl5 $(swig_interface)

perl: $(perl_module)
	perl Makefile.PL
	make -f Makefile

clean:
	rm -f $(programs) $(cpp_objects) $(c_objects) $(shared_library) gma.pm gma_wrap.*
