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
    GDALGetRasterStatistics,
    GDALGetRasterMinimum,
    GDALGetRasterMaximum,
    GDALGetRasterNoDataValue,
    GDALTranslateOptionsNew,
    GDALTranslateOptionsFree,
    GDALTranslate,
    GDALWarpAppOptionsNew,
    GDALWarpAppOptionsSetProgress,
    GDALWarpAppoOptionsFree,
    GDALWarp,
    OSRNewSpatialReference,
    OSRDestroySpatialReference,
    OCTNewCoordinateTransformation,
    OCTTransform;

var progressFuncPtr; // This is going to be a function pointer

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
        GDALGetRasterStatistics = Module.cwrap('GDALGetRasterStatistics', 'number', [
            'number', // GDALRasterBandH
            'number', // int approximations okay?
            'number', // int force?
            'number', // double * min
            'number', // double * max
            'number', // double * mean
            'number', // double * stddev
        ]);
        GDALGetRasterMinimum = Module.cwrap('GDALGetRasterMinimum', 'number', [
            'number', // GDALRasterBandH
            'number', // int * success
        ]);
        GDALGetRasterMaximum = Module.cwrap('GDALGetRasterMaximum', 'number', [
            'number', // GDALRasterBandH
            'number', // int * success
        ]);
        GDALGetRasterNoDataValue = Module.cwrap('GDALGetRasterNoDataValue', 'number', [
            'number', // GDALRasterBandH
            'number', // int * pbSuccess, specifies if nodata value is associated with the layer
        ]);

        GDALTranslate = Module.cwrap('GDALTranslate', 'number', [
            'string', // char * output filename
            'number', // GDALDatasetH dataset to translate
            'number', // GDALTranslateOptions * options object to use
            'number'  // int * pbUsageError
        ]);

        GDALWarpAppOptionsNew = Module.cwrap('GDALWarpAppOptionsNew', 'number', [
            'number', // char ** null-terminated array of option strings as to gdalwarp executable
            'number', // pointer to struct that should usually be null
        ]);
        GDALWarpAppOptionsSetProgress = Module.cwrap('GDALWarpAppOptionsSetProgress', 'number', [
            'number', // GDALWarpAppOptions *
            'number', // GDALProgressFunc
            'number', // void * progress function data
        ]);
        GDALWarpAppOptionsFree = Module.cwrap('GDALWarpAppOptionsFree', 'number', [
            'number', // GDALWarpAppOptions *
        ]);
        GDALWarp = Module.cwrap('GDALWarp', 'number', [
            'string', // Destination dataset path or NULL
            'number', // GDALDatasetH destination dataset or NULL
            'number', // Number of input datasets
            'number', // GDALDatasetH * list of source datasets
            'number', // GDALWarpAppOptions *
            'number', // int * to store errors in if they occur
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

        // Get a reference to a newly allocated SpatialReference object generated based on WKT
        // passed to the constructor.
        OSRNewSpatialReference = Module.cwrap('OSRNewSpatialReference', 'number', ['string']);
        OSRDestroySpatialReference = Module.cwrap('OSRDestroySpatialReference', 'number', [
            'number' // SpatialReferenceH
        ]);
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
        OCTDestroyCoordinateTransformation = Module.cwrap('OCTDestroyCoordinateTransformation', 'number', [
            'number' // CoordinateTransformationH
        ]);
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
        FS.mkdir(PNGPATH);

        progressFuncPtr = Runtime.addFunction(function(progress) {
            //console.log('progress', progress);
            return true; // GDAL will interrupt if this returns false
        });

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
function getProjectedBounds(dataset) {
    /**************************************************************************************
     *                        Coordinates                                                 *
     **************************************************************************************/
    var maxX = GDALGetRasterXSize(dataset);
    var maxY = GDALGetRasterYSize(dataset);
    var geoTransform = jsGDALGetGeoTransform(dataset);
    // We can apply the affine transform to convert from pixel coordinates into geographic coordinates
    // If you wanted to display these on a map, you'd further need to transform to lat/lon, since these
    // are in the raster's CRS.
    var corners = [
        [0, 0],
        [maxX, maxY]
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
    var sourceSrs = OSRNewSpatialReference(GDALGetProjectionRef(dataset));
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
    result = [Array.from(lngLatCoords[0]), Array.from(lngLatCoords[1])];
    Module._free(xCoordOffset);
    Module._free(yCoordOffset);
    OCTDestroyCoordinateTransformation(coordTransform);
    OSRDestroySpatialReference(sourceSrs);
    OSRDestroySpatialReference(targetSrs);
    return result;
}

function getStats(dataset) {
    var bandCount = GDALGetRasterCount(dataset);
    var min, max, noData;
    for (var i = 1; i <= bandCount; i++) {
        var band = GDALGetRasterBand(dataset, i);
        noData = GDALGetRasterNoDataValue(band);
        var newMin = GDALGetRasterMinimum(band, null);
        if (!min || newMin < min) {
            min = newMin;
        }
        var newMax = GDALGetRasterMaximum(band, null);
        if (!max || newMax < max) {
            max = newMax;
        }
    }
    // Assume that all bands have the same no-data value
    var noDataAssignedPtr = Module._malloc(4);
    // Will take a non-zero value if the band has an assigned no-data value
    noData = GDALGetRasterNoDataValue(GDALGetRasterBand(dataset, 1), noDataAssignedPtr);
    var noDataAssigned = Module.getValue(noDataAssignedPtr, 'i32');
    Module._free(noDataAssignedPtr);
    return {
        min: min,
        max: max,
        noData: noDataAssigned !== 0 ? noData : null
    };
}
// Use GDAL functions to create a tile for the specified coordinates
// @param tileObj A tile request with the following fields: upperLeft, lowerRight, coords
// @param dataset An emscripten pointer to an open GDAL dataset
function generateTile(tileObj, dataset) {
    wktStr = GDALGetProjectionRef(dataset);

    /**************************************************************************************
     *                        Thumbnail                                                   *
     **************************************************************************************/
    var uL = tileObj.upperLeft;
    var lR = tileObj.lowerRight;
    var bandCount = GDALGetRasterCount(dataset);
    //console.log('Requested tile bounds', tileObj);
    // The first thing we need is a thumbnail dataset that we can use for warping so that it's fast.
    // Things get a bit ugly passing string arrays to C++ functions. Bear with me.
    var warpOptions = [
        '-s_srs', wktStr,
        '-t_srs', 'EPSG:3857',
        '-te_srs', 'EPSG:4326',
        '-te', Math.min(uL.lng, lR.lng).toString(), Math.min(uL.lat, lR.lat).toString(),
            Math.max(uL.lng, lR.lng).toString(), Math.max(uL.lat, lR.lat).toString(),
        '-ts', '256', '256',
        '-r', 'near',
        '-of', 'GTiff'
    ];
    if (tileObj.stats.noData === null) {
        warpOptions = warpOptions.concat(['-dstnodata', '0']);
    }
    // So first, we need to allocate Emscripten heap space sufficient to store each string
    // as a null-terminated C string.
    var warpPtrsArray = warpOptions.map(function(str) {
        return Module._malloc(Module.lengthBytesUTF8(str) + 1);  // +1 for the null terminator byte
    });

    // In addition to each individual argument being null-terminated, the GDAL docs specify that
    // GDALTranslateOptionsNew take its options passed in as a null-terminated array of pointers,
    // so we have to add on a null (0) byte at the end.
    warpPtrsArray.push(0);
    // Because the C function signature is char **, we'll eventually need to get a pointer to the list of
    // pointers, so we're going to prepare by storing the pointers as a typed array so that we can
    // more easily copy it into heap space later.
    var warpStrPtrs = Uint32Array.from(warpPtrsArray);

    // Next, we need to write each string from the JS string array into the Emscripten heap space
    // we've allocated for it.
    warpOptions.forEach(function(str, i) {
        Module.stringToUTF8(str, warpStrPtrs[i], Module.lengthBytesUTF8(str) + 1);
    });

    // Now, as mentioned above, we also need to copy the pointer array itself into heap space.
    var warpPtrOffset = Module._malloc(warpStrPtrs.length * warpStrPtrs.BYTES_PER_ELEMENT);
    Module.HEAPU32.set(warpStrPtrs, warpPtrOffset/warpStrPtrs.BYTES_PER_ELEMENT);
    // Whew, all finished. ptrOffset is now the address of the start of the list of pointers in
    // Emscripten heap space. Each pointer identifies the address of the start of a parameter
    // string, also stored in heap space. This is the direct equivalent of a char **, which is what
    // GDALWarpAppOptionsNew requires.
    var warpOptionsPtr = GDALWarpAppOptionsNew(warpPtrOffset, null);
    GDALWarpAppOptionsSetProgress(warpOptionsPtr, progressFuncPtr, null);
    Module._free(warpPtrOffset);
    warpPtrsArray.forEach(function(ptr) { Module._free(ptr); });
    // Now that we have our translate options, we need to make a file location to hold the output.
    var warpFilePath = WORKDIR + '/warp.tif';
    // We need a list of datasets to pass to GDALWarp
    var datasetList = Module._malloc(4); // Uint32 pointer
    Module.setValue(datasetList, dataset, '*');
    // And then we can kick off the actual translation process.
    var warpDataset = GDALWarp(warpFilePath, null, 1, datasetList, warpOptionsPtr, null);
    GDALWarpAppOptionsFree(warpOptionsPtr);
    if (warpDataset === 0) { // In other words, a NULL pointer, indicating an error
        postMessage({ tile: { request: tileObj, bytes: new Uint8Array()}});
        return;
    }

    // Now take the warped dataset and convert to PNG
    // See above and the thumbnail example for what's happening here. In a non-example situation this
    // process will need to get pulled out into a function.
    var pngTranslateOptions = [
        '-ot', 'Byte',
        '-of', 'PNG',
        //'-scale', '4000', '15176', '0', '255',  // Good defaults for Landsat
        '-scale', tileObj.stats.min.toString(), tileObj.stats.max.toString(), '0', '255',
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
    GDALTranslateOptionsFree(pngTranslateOptionsPtr);
    Module._free(pngPtrOffset);
    pngPtrsArray.forEach(function(ptr) { Module._free(ptr); });
    // Close out the output dataset before reading from it.
    GDALClose(pngDataset);
    GDALClose(warpDataset);

    // And post results back.
    postMessage({ tile: { request: tileObj, bytes: FS.readFile(pngFilePath, { encoding: 'binary' })}});

    // And cleanup
    // TODO: Make sure everything is cleaned up that can be (there's a lot)
    // TODO: And also figure out a clean way to architect this for wrapper functions
    FS.unmount(WORKDIR);
    FS.unmount(PNGPATH);
}

var activeDataset;

onmessage = function(msg) {
    if (!initialized) {
        console.log('Runtime not initialized yet, try again');
        return;
    }
    var data = msg.data;
    if (data.files) {
        if (activeDataset) {
            GDALClose(activeDataset);
            FS.unmount(TIFFPATH);
        }
        // Make GeoTiffs available to GDAL in the virtual filesystem that it lives inside
        FS.mount(WORKERFS, {
            files: data.files
        }, TIFFPATH);

        activeDataset = GDALOpen(TIFFPATH + '/' + data.files[0].name);
        var bounds = getProjectedBounds(activeDataset);
        var stats = getStats(activeDataset);
        postMessage({ success: true, bounds: bounds, stats: stats });
    } else if (data.tile) {
        // TODO: PostMesage from here not generateTile.
        generateTile(data.tile, activeDataset);
    }
};
