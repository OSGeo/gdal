/*
 * Setup
 */
var TIFFPATH = '/tiffs';
var WORKDIR = '/tmp';
var PNGPATH = '/pngs';

var initialized = false;

var GDALOpen,
    GDALClose,
    GDALGetDriverByName,
    GDALCreate,
    GDALCreateCopy,
    GDALGetGeoTransform,
    GDALSetGeoTransform,
    GDALGetProjectionRef,
    GDALSetProjection,
    GDALGetRasterXSize,
    GDALGetRasterYSize,
    GDALGetRasterDataType,
    GDALGetRasterBand,
    GDALTranslateOptionsNew,
    GDALTranslateOptionsFree,
    GDALSuggestedWarpOutput,
    GDALCreateGenImgProjTransformer,
    GDALGenImgProjTransform,
    OSRNewSpatialReference,
    OCTNewCoordinateTransformation,
    OCTTransform,
    GDALReprojectImage;

var gdalGenImgProjTransformPtr; // This is going to be a function pointer

var EPSG4326 = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4326"]]';
var EPSG3857 = 'PROJCS["WGS 84 / Pseudo-Mercator",GEOGCS["Popular Visualisation CRS",DATUM["Popular_Visualisation_Datum",SPHEROID["Popular Visualisation Sphere",6378137,0,AUTHORITY["EPSG","7059"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6055"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.01745329251994328,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4055"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],PROJECTION["Mercator_1SP"],PARAMETER["central_meridian",0],PARAMETER["scale_factor",1],PARAMETER["false_easting",0],PARAMETER["false_northing",0],AUTHORITY["EPSG","3785"],AXIS["X",EAST],AXIS["Y",NORTH]]';

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
        GDALClose = Module.cwrap('GDALClose', 'number', ['number']);
        GDALGetDriverByName = Module.cwrap('GDALGetDriverByName', 'number', ['string']);
        GDALCreateCopy = Module.cwrap('GDALCreateCopy', 'number', [
                'number', // Pointer to driver
                'string', // Destination filename
                'number', // Pointer to source dataset
                'number', // ? Flag for strictness
                'number', // char ** for options; may be NULL
                'number', // ? Progress function pointer; may be NULL
                'number', // ? Progress data
        ]);
        GDALCreate = Module.cwrap('GDALCreate', 'number', [
                'number', // Pointer to driver
                'string', // Destination filename
                'number', // int X size
                'number', // int Y size
                'number', // int number of bands
                'number', // enum (int) band type
                'number', // char ** options
        ]);
        GDALGetRasterCount = Module.cwrap('GDALGetRasterCount', 'number', ['number']);
        GDALGetRasterXSize = Module.cwrap('GDALGetRasterXSize', 'number', ['number']);
        GDALGetRasterYSize = Module.cwrap('GDALGetRasterYSize', 'number', ['number']);
        GDALGetRasterDataType = Module.cwrap('GDALGetRasterDataType', 'number', [
                'number' // GDALRasterBandH
        ]);
        GDALGetRasterBand = Module.cwrap('GDALGetRasterBand', 'number', [
                'number', // GDALDatasetH
                'number'  // int band number (1-indexed)
        ]);
        GDALTranslate = Module.cwrap('GDALTranslate', 'number', [
            'string', // char * output filename
            'number', // GDALDatasetH dataset to translate
            'number', // GDALTranslateOptions * options object to use
            'number'  // int * pbUsageError
        ]);
        // Params: array of option strings as to gdal_translate; pointer to a struct that should be null.
        GDALTranslateOptionsNew = Module.cwrap('GDALTranslateOptionsNew', 'number', ['number', 'number']);
        GDALTranslateOptionsFree = Module.cwrap('GDALTranslateOptionsFree', 'number', ['number']);
        GDALGetProjectionRef = Module.cwrap('GDALGetProjectionRef', 'string', ['number']);
        GDALSetProjection = Module.cwrap('GDALSetProjection', 'number', [
                'number', // GDALDatasetH the dataset whose projection should be set
                'string'  // char * WKT of projection to set
        ]);
        // Returns an affine transform from geographic coordinate space to geographic coordinate space.
        // Applying this transform to (0,0), (0, maxY), (maxX, maxY), and (maxX, 0) gives us the raster's
        // georeferenced footprint. See http://www.gdal.org/gdal_datamodel.html
        GDALGetGeoTransform = Module.cwrap('GDALGetGeoTransform', 'number', ['number', 'number']);
        GDALSetGeoTransform = Module.cwrap('GDALSetGeoTransform', 'number', [
                'number', // GDALDatasetH the dataset whose geotransform should be set
                'number'  // double * array of 6 double geotransform coefficients
        ]);
        GDALReprojectImage = Module.cwrap('GDALReprojectImage', 'number', [
                'number', // Pointer to source dataset
                'string', // Source WKT
                'number', // Pointer to destination dataset
                'string', // Destination WKT
                'number', // Resample Algorithm enum value
                'number', // Memory limit as a double; 0.0 for default
                'number', // Maximum error allowed for calculating reprojected pixels; 0.0 for exact
                'number', // Callback progress function. Use Runtime.addFunction
                'number', // ? Argument to be passed to progress function.
                'number'  // Warp options object, "normally NULL".
        ]);

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

        GDALCreateGenImgProjTransformer = Module.cwrap(
                'GDALCreateGenImgProjTransformer', 'number', [
                'number', // Source dataset pointer
                'string', // Source WKT char *
                'number', // Destination dataset pointer (or NULL for georeferenced coordinates)
                'string', // Destination WKT char *
                'number', // Whether it is okay to use GCPs if geotransform not available
                'number', // Ignored
                'number', // Maximum order of GCP polynomials. 0 to autoselect; -1 thin plate splines
                ]
                );

        GDALSuggestedWarpOutput = Module.cwrap('GDALSuggestedWarpOutput', 'number', [
                'number', // Source dataset pointer
                'number', // Transformer function pointer
                'number', // Callback data for transformer function (?)
                'number', // double * to six-double array where geotransform will be placed
                'number', // int * to int where columns will be placed
                'number', // int * to int where rows will be placed
        ]);

        GDALGenImgProjTransform = Module.cwrap('GDALGenImgProjTransform', 'number', [
                'number', // void * Pointer to callback data
                'number', // int True: transform destination to source, otherwise source to dest
                'number', // int Count of points in the following arrays
                'number', // double * x coordinates to transform
                'number', // double * y coordinates to transform
                'number', // double * z coordinates to transform
                'number', // int * array of boolean success / fail statuses to write to
        ]);

        // GDALSuggestedWarpOutput takes in a function pointer to a function that performs
        // transformation. This uses Emscripten's function pointer logic to add a pointer to the
        // appropriate function.
        gdalGenImgProjTransformPtr = Runtime.addFunction(GDALGenImgProjTransform);

        // Create a "directory" where user-selected files will be placed
        FS.mkdir(TIFFPATH);
        FS.mkdir(PNGPATH);
        initialized = true;
    }
};

// Load gdal.js. This will populate the Module object, and then call
// Module.onRuntimeInitialized() when it is ready for user code to interact with it.
importScripts('gdal.js');

/* Wrap Emscripten-supplied GDALGetGeoTransform.
 * @param {number} dataset - A number representing a pointer to a dataset on the Emscripten heap.
 * @returns {array} - A 6-item array representing the GDAL GeoTransform object returned.
 */
function jsGDALGetGeoTransform(dataset) {
    // The C function follows a common C pattern where an array to
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
    // Wrap in a "true" Array
    var returnArray = Array.from(geoTransform);
    // Free up what we malloc'ed
    Module._free(affineOffset);
    return returnArray;
}

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

    var dataset = GDALOpen(TIFFPATH + '/' + files[0].name);
    wktStr = GDALGetProjectionRef(dataset);

    // Now that we have the coordinates, we also need to generate a thumbnail and warp it so that we can display it in the bounds.
    // Now pass this back to the calling thread, which is presumably where we'd want to handle it:
    /**************************************************************************************
     *                        Thumbnail                                                   *
     **************************************************************************************/
    var bandCount = GDALGetRasterCount(dataset);
    // The first thing we need is a thumbnail dataset that we can use for warping so that it's fast.
    // Things get a bit ugly passing string arrays to C++ functions. Bear with me.
    var thumbOptions = [
        '-of', 'GTiff',
        '-outsize', '512', '0',
        '-r', 'nearest'
    ];
    // So first, we need to allocate Emscripten heap space sufficient to store each string
    // as a null-terminated C string.
    var thumbPtrsArray = thumbOptions.map(function(str) {
            return Module._malloc(Module.lengthBytesUTF8(str) + 1);  // +1 for the null terminator byte
            });

    // In addition to each individual argument being null-terminated, the GDAL docs specify that
    // GDALTranslateOptionsNew take its options passed in as a null-terminated array of pointers,
    // so we have to add on a null (0) byte at the end.
    thumbPtrsArray.push(0);
    // Because the C function signature is char **, we'll eventually need to get a pointer to the list of
    // pointers, so we're going to prepare by storing the pointers as a typed array so that we can
    // more easily copy it into heap space later.
    var thumbStrPtrs = Uint32Array.from(thumbPtrsArray);

    // Next, we need to write each string from the JS string array into the Emscripten heap space
    // we've allocated for it.
    thumbOptions.forEach(function(str, i) {
        Module.stringToUTF8(str, thumbStrPtrs[i], Module.lengthBytesUTF8(str) + 1);
    });

    // Now, as mentioned above, we also need to copy the pointer array itself into heap space.
    var thumbPtrOffset = Module._malloc(thumbStrPtrs.length * thumbStrPtrs.BYTES_PER_ELEMENT);
    Module.HEAPU32.set(thumbStrPtrs, thumbPtrOffset/thumbStrPtrs.BYTES_PER_ELEMENT);
    // Whew, all finished. ptrOffset is now the address of the start of the list of pointers in
    // Emscripten heap space. Each pointer identifies the address of the start of a parameter
    // string, also stored in heap space. This is the direct equivalent of a char **, which is what
    // GDALTranslateOptionsNew requires.
    var thumbTranslateOptionsPtr = GDALTranslateOptionsNew(thumbPtrOffset, null);
    // Now that we have our translate options, we need to make a file location to hold the output.
    var thumbnailFilePath = WORKDIR + '/thumb.tif';
    // And then we can kick off the actual translation process.
    var thumbDataset = GDALTranslate(thumbnailFilePath, dataset, thumbTranslateOptionsPtr, null);
    console.log('Finished thumbnail');
    var thumbAffine = jsGDALGetGeoTransform(thumbDataset);

    /**************************************************************************************
     *                        Warp                                                        *
     **************************************************************************************/

    // First, we need to figure out what the bounds and transformation of the new image need to be.
    // GDALCreateGenImgProjTransformer gives us the transformer from the source CRS to 3857
    var projTransformArg = GDALCreateGenImgProjTransformer(
        thumbDataset,
        wktStr,
        null,
        EPSG3857,
        0, // False
        0, // Ignored
        1 // This will probably be ignored in most cases as well
    );
    // SuggestedWarpOutput writes data into pointers, so we need to allocate some.
    var warpedSuggestedColsPtr = Module._malloc(4); // Int32
    var warpedSuggestedRowsPtr = Module._malloc(4); // Int32
    var warpedGeoTransformPtr = Module._malloc(6 * Float64Array.BYTES_PER_ELEMENT);
    // GDALSuggestedWarpOutput then gives us the x/y size and geotransform for what the dataset
    // needs to be.
    GDALSuggestedWarpOutput(
        thumbDataset,
        gdalGenImgProjTransformPtr,
        projTransformArg,
        warpedGeoTransformPtr,
        warpedSuggestedColsPtr,
        warpedSuggestedRowsPtr
    );
    // Next we GDALCreate a new dataset using the MEM driver with the suggested size and transform
    var memDriver = GDALGetDriverByName('MEM');
    var warpDataset = GDALCreate(
        memDriver,
        "warped",
        Module.getValue(warpedSuggestedColsPtr, 'i32'),
        Module.getValue(warpedSuggestedRowsPtr, 'i32'),
        bandCount,
        GDALGetRasterDataType(GDALGetRasterBand(dataset, 1)),
        null
    );
    // And then we set the CRS for the new dataset using GDALSetProjection
    GDALSetProjection(warpDataset, EPSG3857);
    // And similarly set the geotransform on the new dataset.
    GDALSetGeoTransform(warpDataset, warpedGeoTransformPtr);

    warpedAffine = jsGDALGetGeoTransform(warpDataset);
    // By the time we get here, we need to have a warpDataset that has the correct size,
    // geotransform, and CRS that the source dataset would have after being transformed. The
    // reprojection then just takes care of copying over the pixel values.
    GDALReprojectImage(
        thumbDataset,
        wktStr,
        warpDataset,
        EPSG3857,
        0, // Nearest Neighbor
        0.0,
        0.0, // TODO: See how much we can speed up with higher numbers
        null,
        null,
        null
    );
    GDALClose(thumbDataset);
    console.log('Finished warping');
    /**************************************************************************************
     *                        Coordinates                                                 *
     **************************************************************************************/
    // Now, generate the new bounds from the warped dataset for map display
    var maxX = GDALGetRasterXSize(warpDataset);
    var maxY = GDALGetRasterYSize(warpDataset);
    var geoTransform = jsGDALGetGeoTransform(warpDataset);
    // We can apply the affine transform to convert from pixel coordinates into geographic coordinates
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
    var sourceSrs = OSRNewSpatialReference(GDALGetProjectionRef(warpDataset));
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
    // Now take the warped dataset and convert to PNG
    // See above and the thumbnail example for what's happening here. In a non-example situation this
    // process will need to get pulled out into a function.
    var pngTranslateOptions = [
        '-ot', 'Byte',
        '-of', 'PNG',
        '-scale'
    ];
    // Dynamically adjust band output based on availability
    for (var i = 1; i <= 3 && i <= bandCount; i++) {
        pngTranslateOptions.push('-b');
        pngTranslateOptions.push(i.toString());
    }
    var pngPtrsArray = pngTranslateOptions.map(function(str) {
        return Module._malloc(Module.lengthBytesUTF8(str) + 1);  // +1 for the null terminator byte
    });
    pngPtrsArray.push(0);
    var pngStrPtrs = Uint32Array.from(pngPtrsArray);
    pngTranslateOptions.forEach(function(str, i) {
        Module.stringToUTF8(str, pngStrPtrs[i], Module.lengthBytesUTF8(str) + 1);
    });
    var pngPtrOffset = Module._malloc(pngStrPtrs.length * pngStrPtrs.BYTES_PER_ELEMENT);
    Module.HEAPU32.set(pngStrPtrs, pngPtrOffset/pngStrPtrs.BYTES_PER_ELEMENT);
    var pngTranslateOptionsPtr = GDALTranslateOptionsNew(pngPtrOffset, null);
    var pngFilePath = PNGPATH + '/thumb.png';
    var pngDataset = GDALTranslate(pngFilePath, warpDataset, pngTranslateOptionsPtr, null);
    console.log('Finished writing PNG');
    // Close out the output dataset before reading from it.
    GDALClose(pngDataset);
    GDALClose(warpDataset);

    // And post results back.
    postMessage({ coords: lngLatCoords, bytes: FS.readFile(pngFilePath, { encoding: 'binary' })});

    // And cleanup
    // TODO: Make sure everything is cleaned up that can be (there's a lot)
    // TODO: And also figure out a clean way to architect this for wrapper functions
    FS.unmount(TIFFPATH);
    FS.unmount(WORKDIR);
    FS.unmount(PNGPATH);
    GDALClose(dataset);
    Module._free(thumbPtrOffset);
    Module._free(affineOffset);
    Module._free(xCoordOffset);
    Module._free(yCoordOffset);
}

// Assume that all incoming messages are FileLists of GeoTiffs and inspect them.
onmessage = function(msg) {
    if (!initialized) {
        console.log('Runtime not initialized yet, try again');
        return;
    }
    inspectTiff(msg.data);
};
