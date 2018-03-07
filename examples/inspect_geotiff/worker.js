/*
 * Setup
 */
var TIFFPATH = '/tiffs';

var initialized = false;

var GDALOpen,
    GDALClose,
    GDALGetRasterCount,
    GDALGetRasterXSize,
    GDALGetRasterYSize,
    GDALGetProjectionRef,
    GDALGetGeoTransform,
    CPLQuietErrorHandler,
    CPLSetErrorHandler,
    CPLGetLastErrorMsg,
    CPLGetLastErrorNo,
    CPLGetLastErrorType,
    CPLErrorReset;

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
        GDALClose = Module.cwrap('GDALClose', null, ['number']);
        CPLErrorReset = Module.cwrap('CPLErrorReset', null, []);
        CPLSetErrorHandler = Module.cwrap('CPLSetErrorHandler', 'number', ['number']);
        CPLQuietErrorHandler = Module.cwrap('CPLQuietErrorHandler', null, ['number', 'number', 'number']);
        var cplQuietFnPtr = Runtime.addFunction(CPLQuietErrorHandler);
        CPLGetLastErrorNo = Module.cwrap('CPLGetLastErrorNo', 'number', []);
        CPLGetLastErrorMsg = Module.cwrap('CPLGetLastErrorMsg', 'string', []);
        CPLGetLastErrorType = Module.cwrap('CPLGetLastErrorType', 'number', []);
        GDALGetRasterCount = Module.cwrap('GDALGetRasterCount', 'number', ['number']);
        GDALGetRasterXSize = Module.cwrap('GDALGetRasterXSize', 'number', ['number']);
        GDALGetRasterYSize = Module.cwrap('GDALGetRasterYSize', 'number', ['number']);
        GDALGetProjectionRef = Module.cwrap('GDALGetProjectionRef', 'string', ['number']);
        // Returns an affine transform from geographic coordinate space to geographic coordinate space.
        // Applying this transform to (0,0), (0, maxY), (maxX, maxY), and (maxX, 0) gives us the raster's
        // georeferenced footprint. See http://www.gdal.org/gdal_datamodel.html
        GDALGetGeoTransform = Module.cwrap('GDALGetGeoTransform', 'number', ['number', 'number']);

        CPLSetErrorHandler(cplQuietFnPtr);
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
// Use GDAL functions to provide information about a list of files.
// @param files a FileList object as returned by a file input's .files field
function inspectTiff(files) {
    // Make GeoTiffs available to GDAL in the virtual filesystem that it lives inside
    FS.mount(WORKERFS, {
        files: files
    }, TIFFPATH);

    var results = [];
    CPLErrorReset();
    for(var i = 0; i < files.length; i++) {
        // Create a GDAL Dataset
        var dataset = GDALOpen(TIFFPATH + '/' + files[i].name);
        if (CPLGetLastErrorNo() !== 0) {
            console.log('Error occurred opening dataset');
            console.log('Error message: ', CPLGetLastErrorMsg());
            console.log('Error number: ', CPLGetLastErrorNo());
            console.log('Resetting errors');
            CPLErrorReset();
            continue;
        }
        var bandCount = GDALGetRasterCount(dataset);
        var maxX = GDALGetRasterXSize(dataset);
        var maxY = GDALGetRasterYSize(dataset);
        wktStr = GDALGetProjectionRef(dataset);
        // This is where things get a bit hairy; the C function follows a common C pattern where an array to
        // store the results is allocated and passed into the function, which populates the array with the
        // results. Emscripten supports passing arrays to functions, but it always creates a *copy* of the
        // array, which means that the original JS array remains unchanged, which isn't what we want in this
        // case. So first, we have to malloc an array inside the Emscripten heap with the correct size. In this
        // case that is 6 because the GDAL affine transform array has six elements.
        var byteOffset = Module._malloc(6 * Float64Array.BYTES_PER_ELEMENT);
        // byteOffset is now a pointer to the start of the double array in Emscripten heap space
        // GDALGetGeoTransform dumps 6 values into the passed double array.
        GDALGetGeoTransform(dataset, byteOffset);
        // Module.HEAPF64 provides a view into the Emscripten heap, as an array of doubles. Therefore, our byte offset
        // from _malloc needs to be converted into a double offset, so we divide it by the number of bytes per double,
        // and then get a subarray of those six elements off the Emscripten heap.
        var geoTransform = Module.HEAPF64.subarray(
            byteOffset/Float64Array.BYTES_PER_ELEMENT,
            byteOffset/Float64Array.BYTES_PER_ELEMENT + 6
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
        results.push({
            bandCount: bandCount,
            xSize: maxX,
            ySize: maxY,
            projectionWkt: wktStr,
            coordinateTransform: geoTransform,
            corners: geoCorners
        });

        GDALClose(dataset);
    }

    // Now pass this back to the calling thread, which is presumably where we'd want to handle it:
    postMessage(results);
 
    // And cleanup
    FS.unmount(TIFFPATH);
}

// Assume that all incoming messages are FileLists of GeoTiffs and inspect them.
onmessage = function(msg) {
    if (!initialized) {
        console.log('Runtime not initialized yet, try again');
        return;
    }
    inspectTiff(msg.data);
};
