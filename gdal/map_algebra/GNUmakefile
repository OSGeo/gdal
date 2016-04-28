cflags = -g

inc = /home/ajolma/usr/include

gdal = -L/home/ajolma/usr/lib -lgdal

gma = -L/home/ajolma/github/gdal/gdal/map_algebra -lgma

cpp_objects = gma_classes.o

cpp_headers = gdal_map_algebra_core.h gdal_map_algebra_classes.hpp gdal_map_algebra.hpp gma_classes.hpp gma_band.hpp gma_two_bands.hpp

shared_library = libgma.so

programs = test demo histogram d8

all: $(programs)
	cd perl; make all; cd ..
	cd python; make all; cd ..

test: test.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc $(cflags) -o test -I $(inc) test.cpp $(gma) $(gdal) -lstdc++ -lm

demo: demo.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc $(cflags) -o demo -I $(inc) demo.cpp $(gma) $(gdal) -lstdc++ -lm

histogram: histogram.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc $(cflags) -o histogram -I $(inc) histogram.cpp $(gma) $(gdal) -lstdc++ -lm

d8: d8.cpp $(cpp_objects) $(cpp_headers) $(shared_library)
	gcc $(cflags) -o d8 -I $(inc) d8.cpp $(gma) $(gdal) -lstdc++ -lm

gma_classes.o: gma_classes.cpp $(cpp_headers)
	gcc -c $(cflags) -Wall -Werror -fpic -I $(inc) gma_classes.cpp

$(shared_library): gma_classes.o
	gcc -shared -o $(shared_library) gma_classes.o

clean:
	rm -f $(programs) $(cpp_objects) $(shared_library)
