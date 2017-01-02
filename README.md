GDAL JS
==============
An [Emscripten](https://github.com/kripken/emscripten) port of [GDAL](http://www.gdal.org).

Developing
-----------
1. Install Emscripten and its dependencies following the instructions
[here](https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html).
2. If you are on a Mac, you may need to run the following if you receive errors like this: `env:
python2: No such file or directory`
```
./emsdk install sdk-incoming-64bit --build=Release
./emsdk activate sdk-incoming-64bit
```
3. From the project directory, run `make gdal-lib`

Usage
---------------
There's not really much you can easily do right now; making a more ergonomic interface to the GDAL C
API is high on my to-do list.

If you're feeling adventurous, Emscripten provides a debugging interface for command-line programs
which I've used successfully with `gdalinfo`. You should be able to compile `gdalinfo`, for example,
by doing something like this from the `gdal` directory:
```
emmake make apps-target
cd apps
em++ -s ALLOW_MEMORY_GROWTH=1 gdalinfo_bin.o  ../libgdal.a
-L../../proj4/src/.libs -lproj -lpthread -ldl -o gdalinfo.html
```
Note that you will need to preload a useful file (a GeoTiff) into the Emscripten file system
and pass it to `gdalinfo` as a command line argument in order for this to do anything useful. The
Emscripten docs contain information on how to do both of those things.
