/*
 * Setup
 */
var TIFFPATH = '/tiffs';

var initialized = false;

var GDALOpen,
    GDALGetGeoTransform,
    GDALGetProjectionRef,
    GDALGetRasterXSize,
    GDALGetRasterYSize,
    OSRNewSpatialReference,
    OCTNewCoordinateTransformation,
    OCTTransform;

var EPSG4326 = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]';

// Set up Module object for gdal.js to populate. Emscripten sets up its compiled
// code to look for a Module object in the global scope. If found, it reads runtime
// configuration from the existing object, and then further populates that object
// with other helpful functionality (e.g. ccall() and cwrap(), which are used in
// the onRuntimeInitialized callback, below).
var Module = {
    'print': function(text) { console.log('stdout: ' + text); },
    'printErr': function(text) { console.log('stderr: ' + text); },
    // Optimized builds contain a .js.mem file which is loaded asynchronously;
    // this waits until that has finished before performing further setup.
    'onRuntimeInitialized': function() {
        // Initialize GDAL
        Module.ccall('GDALAllRegister', null, [], []);

        // Set up JS proxy functions
        // Note that JS Number types are used to represent pointers, which means that
        // any time we want to pass a pointer to an object, such as in GDALOpen, which in
        // C returns a pointer to a GDALDataset, we need to use 'number'.
        GDALOpen = Module.cwrap('GDALOpen', 'number', ['string']);
        GDALGetRasterCount = Module.cwrap('GDALGetRasterCount', 'number', ['number']);
        GDALGetRasterXSize = Module.cwrap('GDALGetRasterXSize', 'number', ['number']);
        GDALGetRasterYSize = Module.cwrap('GDALGetRasterYSize', 'number', ['number']);
        GDALGetProjectionRef = Module.cwrap('GDALGetProjectionRef', 'string', ['number']);
        // Returns an affine transform from geographic coordinate space to geographic coordinate space.
        // Applying this transform to (0,0), (0, maxY), (maxX, maxY), and (maxX, 0) gives us the raster's
        // georeferenced footprint. See http://www.gdal.org/gdal_datamodel.html
        GDALGetGeoTransform = Module.cwrap('GDALGetGeoTransform', 'number', ['number', 'number']);

        // Get a reference to a newly allocated SpatialReference object generated based on WKT
        // passed to the constructor.
        OSRNewSpatialReference = Module.cwrap('OSRNewSpatialReference', 'number', ['string']);
        // Get a reference to a newly allocated SpatialReference object generated based on the EPSG
        // code passed to the constructor.
        OSRImportFromEPSG = Module.cwrap('OSRImportFromEPSG', 'number', ['number']);
        // Get a reference to a newly allocated CoordinateTransformation object which transforms
        // from the source SpatialReference (1st param) to the target SpatialReference (2nd param)
        OCTNewCoordinateTransformation = Module.cwrap(
            'OCTNewCoordinateTransformation',
            'number',
            ['number', 'number']
        );
        // Transform arrays of coordinates in-place
        // Params are:
        // 1. Coordinate transformation to use
        // 2. Number of coordinates to transform
        // 3. Array of X coordinates to transform
        // 4. Array of Y coordinates to transform
        // 5. Array of Z coordinates to transform
        OCTTransform = Module.cwrap(
            'OCTTransform',
            'number',
            ['number', 'number', 'number', 'number', 'number']
        );

        // Create a "directory" where user-selected files will be placed
        FS.mkdir(TIFFPATH);
        initialized = true;
    }
};

// Load gdal.js. This will populate the Module object, and then call
// Module.onRuntimeInitialized() when it is ready for user code to interact with it.
importScripts('gdal.js');

/*
 * Logic
 */
// Use GDAL functions to provide information about a single file
// @param files a FileList object as returned by a file input's .files field
function inspectTiff(files) {
    // Make GeoTiffs available to GDAL in the virtual filesystem that it lives inside
    FS.mount(WORKERFS, {
        files: files
    }, TIFFPATH);
    
    // Create a GDAL Dataset
    var dataset = GDALOpen(TIFFPATH + '/' + files[0].name);
    var maxX = GDALGetRasterXSize(dataset);
    var maxY = GDALGetRasterYSize(dataset);
    wktStr = GDALGetProjectionRef(dataset);
    // This is where things get a bit hairy; the C function follows a common C pattern where an array to
    // store the results is allocated and passed into the function, which populates the array with the
    // results. Emscripten supports passing arrays to functions, but it always creates a *copy* of the
    // array, which means that the original JS array remains unchanged, which isn't what we want in this
    // case. So first, we have to malloc an array inside the Emscripten heap with the correct size. In this
    // case that is 6 because the GDAL affine transform array has six elements.
    var affineOffset = Module._malloc(6 * Float64Array.BYTES_PER_ELEMENT);
    // byteOffset is now a pointer to the start of the double array in Emscripten heap space
    // GDALGetGeoTransform dumps 6 values into the passed double array.
    GDALGetGeoTransform(dataset, affineOffset);
    // Module.HEAPF64 provides a view into the Emscripten heap, as an array of doubles. Therefore, our byte offset
    // from _malloc needs to be converted into a double offset, so we divide it by the number of bytes per double,
    // and then get a subarray of those six elements off the Emscripten heap.
    var geoTransform = Module.HEAPF64.subarray(
        affineOffset/Float64Array.BYTES_PER_ELEMENT,
        affineOffset/Float64Array.BYTES_PER_ELEMENT + 6
    );
    // Finally, we can apply the affine transform to convert from pixel coordinates into geographic coordinates
    // If you wanted to display these on a map, you'd further need to transform to lat/lon, since these
    // are in the raster's CRS.
    var corners = [
        [0, 0],
        [maxX, 0],
        [maxX, maxY],
        [0, maxY]
    ];
    var geoCorners = corners.map(function(coords) {
        var x = coords[0];
        var y = coords[1];
        return [
            // http://www.gdal.org/gdal_datamodel.html
            geoTransform[0] + geoTransform[1]*x + geoTransform[2]*y,
            geoTransform[3] + geoTransform[4]*x + geoTransform[5]*y
       ];
    });

    // Now that we have our corners in geospatial coordinates, we need to transform them into lat-lon,
    // which is what Leaflet expects.
    // First, construct a SpatialReference using the dataset's ProjectionRef WKT
    var sourceSrs = OSRNewSpatialReference(wktStr);
    // Next, we also need an SRS for Lat/Lon
    var targetSrs = OSRNewSpatialReference(EPSG4326);
    // Now we can create a CoordinateTransformation object to transform between the two
    var coordTransform = OCTNewCoordinateTransformation(sourceSrs, targetSrs);
    // And lastly, we can transform the Xs and Ys. This requires a similar malloc process to the
    // affine transform function above, since the coordinates are transformed in-place
    var xCoords = new Float64Array(geoCorners.map(function(coords) { return coords[0]; }));
    var yCoords = new Float64Array(geoCorners.map(function(coords) { return coords[1]; }));
    var xCoordOffset = Module._malloc(xCoords.length * xCoords.BYTES_PER_ELEMENT);
    var yCoordOffset = Module._malloc(yCoords.length * yCoords.BYTES_PER_ELEMENT);
    // But this time we copy into the memory space from our external array
    Module.HEAPF64.set(xCoords, xCoordOffset/xCoords.BYTES_PER_ELEMENT);
    Module.HEAPF64.set(yCoords, yCoordOffset/yCoords.BYTES_PER_ELEMENT);
    // Z is null in this case.
    var res = OCTTransform(coordTransform, xCoords.length, xCoordOffset, yCoordOffset, null);
    // Pull out the coordinates
    var lngLatCoords = [
        Module.HEAPF64.subarray(
            xCoordOffset/xCoords.BYTES_PER_ELEMENT,
            xCoordOffset/xCoords.BYTES_PER_ELEMENT + xCoords.length
        ),
        Module.HEAPF64.subarray(
            yCoordOffset/yCoords.BYTES_PER_ELEMENT,
            yCoordOffset/yCoords.BYTES_PER_ELEMENT + yCoords.length
        )
    ];
    // Now pass this back to the calling thread, which is presumably where we'd want to handle it:
    postMessage(lngLatCoords);
 
    // And cleanup
    FS.unmount(TIFFPATH);
    Module._free(affineOffset);
    Module._free(xCoordOffset);
    Module._free(yCoordOffset);
    // TODO: Close GDAL Dataset
    // Deallocate SRSes and CoordinateTransforms
}

// Assume that all incoming messages are FileLists of GeoTiffs and inspect them.
onmessage = function(msg) {
    if (!initialized) {
        console.log('Runtime not initialized yet, try again');
        return;
    }
    inspectTiff(msg.data);
};
