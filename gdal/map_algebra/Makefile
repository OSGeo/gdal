inc = /home/ajolma/usr/include

gdal = -L /home/ajolma/usr/lib -lgdal

objects = gdal_map_algebra_arg.o gdal_map_algebra_compute_value.o gdal_map_algebra_simple.o gdal_map_algebra_two_bands.o gma_band.o gma_hash.o

headers = gma_hash.h gdal_map_algebra_private.h gdal_map_algebra.h

all: demo d8 histogram

demo: demo.cpp $(objects)
	gcc -o demo -I $(inc) demo.cpp $(objects) $(gdal) -g -lstdc++ -lm

d8: d8.cpp $(objects)
	gcc -o d8 -I $(inc) d8.cpp $(objects) $(gdal) -g -lstdc++ -lm

histogram: histogram.cpp $(objects)
	gcc -o histogram -I $(inc) histogram.cpp $(objects) $(gdal) -g -lstdc++ -lm

gdal_map_algebra_arg.o: $(headers) gdal_map_algebra_arg.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_arg.cpp

gdal_map_algebra_compute_value.o: $(headers) gdal_map_algebra_compute_value.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_compute_value.cpp

gdal_map_algebra_simple.o: $(headers) gdal_map_algebra_simple.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_simple.cpp

gdal_map_algebra_two_bands.o: $(headers) gdal_map_algebra_two_bands.cpp
	gcc -c -g -I $(inc) gdal_map_algebra_two_bands.cpp

gma_band.o: $(headers) gma_band.cpp
	gcc -c -g -I $(inc) gma_band.cpp

gma_hash.o: $(headers) gma_hash.cpp
	gcc -c -g -I $(inc) gma_hash.cpp

clean:
	rm -f $(objects) demo d8 histogram
