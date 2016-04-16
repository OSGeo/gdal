inc = /home/ajolma/usr/include

gdal = -L /home/ajolma/usr/lib -lgdal

objects = gdal_map_algebra_arg.o gdal_map_algebra_compute_value.o gdal_map_algebra_simple.o gdal_map_algebra_two_bands.o gdal_map_algebra_hash.o gdal_map_algebra_help.o

headers = hash.hpp private.hpp gdal_map_algebra.hpp gdal_map_algebra.h

all: demo d8 histogram test test-c-api

demo: demo.cpp $(objects)
	gcc -o demo -I $(inc) demo.cpp $(objects) $(gdal) -g -lstdc++ -lm

d8: d8.cpp $(objects)
	gcc -o d8 -I $(inc) d8.cpp $(objects) $(gdal) -g -lstdc++ -lm

histogram: histogram.cpp $(objects)
	gcc -o histogram -I $(inc) histogram.cpp $(objects) $(gdal) -g -lstdc++ -lm

test: test.cpp $(objects)
	gcc -o test -I $(inc) test.cpp $(objects) $(gdal) -g -lstdc++ -lm

test-c-api: test-c-api.c $(objects) gdal_map_algebra_c_api.o
	gcc -o test-c-api -I $(inc) test-c-api.c gdal_map_algebra_c_api.o $(objects) $(gdal) -g -lstdc++ -lm

gdal_map_algebra_arg.o: $(headers) gdal_map_algebra_arg.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_arg.cpp

gdal_map_algebra_compute_value.o: $(headers) gdal_map_algebra_compute_value.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_compute_value.cpp

gdal_map_algebra_simple.o: $(headers) gdal_map_algebra_simple.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_simple.cpp

gdal_map_algebra_two_bands.o: $(headers) gdal_map_algebra_two_bands.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_two_bands.cpp

gdal_map_algebra_hash.o: $(headers) gdal_map_algebra_hash.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_hash.cpp

gdal_map_algebra_help.o: $(headers) gdal_map_algebra_help.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_help.cpp

gdal_map_algebra_c_api.o: $(headers) gdal_map_algebra_c_api.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_c_api.cpp

clean:
	rm -f $(objects) demo d8 histogram test
