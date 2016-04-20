inc = /home/ajolma/usr/include

gdal = -L /home/ajolma/usr/lib -lgdal

cpp_objects = gma_classes.o

c_objects = gma_c_api.o

cpp_headers = gdal_map_algebra_core.h gdal_map_algebra_classes.hpp gdal_map_algebra.hpp gma_classes.hpp gma_band.hpp gma_two_bands.hpp

c_headers = gdal_map_algebra_core.h gdal_map_algebra_types.h gdal_map_algebra.h

programs = test demo histogram d8 test-c-api

all: $(programs)

test: test.cpp $(cpp_objects) $(cpp_headers)
	gcc -o test -I $(inc) test.cpp $(cpp_objects) $(gdal) -g -lstdc++ -lm

demo: demo.cpp $(cpp_objects) $(cpp_headers)
	gcc -o demo -I $(inc) demo.cpp $(cpp_objects) $(gdal) -g -lstdc++ -lm

histogram: histogram.cpp $(cpp_objects) $(cpp_headers)
	gcc -o histogram -I $(inc) histogram.cpp $(cpp_objects) $(gdal) -g -lstdc++ -lm

d8: d8.cpp $(cpp_objects) $(cpp_headers)
	gcc -o d8 -I $(inc) d8.cpp $(cpp_objects) $(gdal) -g -lstdc++ -lm

test-c-api: test-c-api.c $(c_objects) $(cpp_objects)
	gcc -o test-c-api -I $(inc) test-c-api.c $(c_objects) $(cpp_objects) $(gdal) -g -lstdc++ -lm

gma_classes.o: gma_classes.cpp $(cpp_headers)
	gcc -c -g -I $(inc) gma_classes.cpp

gma_c_api.o: gma_c_api.cpp $(c_headers)
	gcc -c -g -I $(inc) gma_c_api.cpp $(cpp_objects)

clean:
	rm -f $(programs) $(cpp_objects) $(c_objects)
