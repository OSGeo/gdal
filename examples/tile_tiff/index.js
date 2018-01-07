// TODO: This is abusing closures; refactor and make variable scoping nicer.

var tiler = new Worker('worker.js');

var map = L.map('leaflet').setView([0,0],3);

var tileCallbacks = {};

// Calculated min/max/nodata for the file, used for each tile request
var fileStats;

L.GridLayer.WorkerTiles = L.GridLayer.extend({
    createTile: function(coords, done) {
        var uLPix = {
            x: coords.x * 256, // In real life, "this.getTileSize()"
            y: coords.y * 256
        };
        var lRPix = {
            x: uLPix.x + 256,
            y: uLPix.y + 256
        }; // Ditto
        
        var map = this._map; // TODO: Don't rely on Leaflet internals
        var uLGeo = map.unproject(uLPix, coords.z);
        var lRGeo = map.unproject(lRPix, coords.z);

        var tile = document.createElement('img');
        tiler.postMessage({ tile: {
            upperLeft: uLGeo,
            lowerRight: lRGeo,
            coords: coords,
            stats: fileStats
        }});
        var callback = function(bytes) {
            // This doesn't really seem to make a difference, but it's quicker.
            // TODO: Make empty tiles not show up as broken images
            if (bytes.length === 0) {
                done(null, null);
            } else {
                var outputBlob = new Blob([bytes], { type: 'image/png' });
                var imageURL = window.URL.createObjectURL(outputBlob);
                tile.src = imageURL;
                done(null, tile); // done(error, tile);
            }
        }

        var callbackKey = coords.x.toString() + ',' + coords.y.toString() + ',' + coords.z.toString();
        tileCallbacks[callbackKey] = callback;
        return tile;
    }
});

L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="http://osm.org/copyright">OpenStreetMap</a> contributors',
    maxZoom: 18,
}).addTo(map);

var tiffTiles;

function openFile() {
    var files = document.querySelector('#geotiff-select').files;
    tiler.postMessage({files: files});
}

tiler.onmessage = function(evt) {
    if (evt.data.tile) {
        var tileReq = evt.data.tile.request;
        var callbackKey = (
            tileReq.coords.x.toString() + ',' +
            tileReq.coords.y.toString() + ',' +
            tileReq.coords.z.toString()
        );
        tileCallbacks[callbackKey](evt.data.tile.bytes);
        delete tileCallbacks[callbackKey];
    } else if (evt.data.success) {
        if (tiffTiles) {
            tiffTiles.remove();
        }
        tiffTiles = new L.GridLayer.WorkerTiles();
        var lats = Array.from(evt.data.bounds[1]);
        var lngs = Array.from(evt.data.bounds[0]);

        // TODO: Remove globals
        fileStats = evt.data.stats;
        // Zip
        var latLngs = lats.map(function(lat, i, arr) {
            return new Array(lat, lngs[i]);
        });
        map.fitBounds(latLngs);
        tiffTiles.addTo(map);
    } else {
        console.log(evt);
    }
};
