The examples in this folder show how to use the Emscripten generated GDAL library in increasingly
complex ways. Ordered from easiest to hardest, they are roughly:

1. `inspect_geotiff` (Show info about a GeoTIFF, roughly mimicking `gdalinfo`
2. `map_extent` (Display the GeoTIFF's extent on a Leaflet map)
3. `thumbnail` (Generate false-color PNG thumbnail of first three bands of a GeoTIFF)
4. `thumbnail_map` (Generate false-color PNG thumbnail, warp to EPSG:3857, display on Leaflet map)

The examples build on each other, but in order to keep them as instructive as possible, they
deliberately repeat common elements, rather than pulling them out into wrapper functions. The reason
for this is that interacting with Emscripten-generated functions often involves direct access to the
Emscripten heap and manipulating pointers, so simply documenting function signatures may not provide
a complete picture of how to use a given function. Rather, there is often a significant amount of
boilerplate necessary in order to interact with Emscripten functions. The goal of these examples is
for each one to provide a standalone picture of how this boilerplate works. Writing a wrapper
library would get in the way of demonstrating these core concepts. Much of the code in these
examples could form the _foundation_ of a more ergonomic and idiomatic wrapper library, but
generating that library is not the purpose of these examples. Rather, it is to document the usage
patterns necessary for interacting with the Emscripten-generated GDAL API at the lowest level.

In order to run these examples, make sure that `gdal.js`, `gdal.js.mem`, and `gdal.data` are
available from the example directory (there are symlinks to these locations in the root of the
repository for convenience). Then simply serve the directory from a webserver such as
`python -m SimpleHTTPServer`.
