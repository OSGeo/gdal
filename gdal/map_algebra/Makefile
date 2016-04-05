all: gdal_map_algebra_arg.o gdal_map_algebra_compute_value.o gdal_map_algebra_simple.o gdal_map_algebra_two_bands.o gma_band.o gma_hash.o
	gcc -o demo -I /home/ajolma/usr/include demo.cpp *.o -L /home/ajolma/usr/lib  -lgdal -g -lstdc++ -lm
	gcc -o d8 -I /home/ajolma/usr/include d8.cpp *.o -L /home/ajolma/usr/lib  -lgdal -g -lstdc++ -lm
	gcc -o histogram -I /home/ajolma/usr/include histogram.cpp *.o -L /home/ajolma/usr/lib  -lgdal -g -lstdc++ -lm

gdal_map_algebra_arg.o: gma_hash.h gdal_map_algebra_arg.cpp
	gcc -c -I /home/ajolma/usr/include gdal_map_algebra_arg.cpp

gdal_map_algebra_compute_value.o: gma_hash.h gdal_map_algebra_compute_value.cpp
	gcc -c -I /home/ajolma/usr/include gdal_map_algebra_compute_value.cpp

gdal_map_algebra_simple.o: gma_hash.h gdal_map_algebra_simple.cpp
	gcc -c -I /home/ajolma/usr/include gdal_map_algebra_simple.cpp

gdal_map_algebra_two_bands.o: gma_hash.h gdal_map_algebra_two_bands.cpp
	gcc -c -I /home/ajolma/usr/include gdal_map_algebra_two_bands.cpp

gma_band.o: gma_hash.h gma_band.cpp
	gcc -c -I /home/ajolma/usr/include gma_band.cpp

gma_hash.o: gma_hash.h gma_hash.cpp
	gcc -c -I /home/ajolma/usr/include gma_hash.cpp
