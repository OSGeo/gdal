var thumbnailer = new Worker('worker.js');

var map = L.map('leaflet').setView([0,0],3);

L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="http://osm.org/copyright">OpenStreetMap</a> contributors',
    maxZoom: 18,
}).addTo(map);

var overlay = null;

function makeThumbnail() {
    var files = document.querySelector('#geotiff-select').files;
    thumbnailer.postMessage(files);
}

thumbnailer.onmessage = function(evt) {
    displayMapImage(evt.data);
};

function displayMapImage(data) {
    if (overlay !== null) {
        overlay.remove();
    }
    var imageBytes = data.bytes;
    var coords = data.coords;
    var outputBlob = new Blob([imageBytes], { type: 'image/png' });
    var imageURL = window.URL.createObjectURL(outputBlob);
    var lats = Array.from(coords[1]);
    var lngs = Array.from(coords[0]);

    // Zip
    var latLngs = lats.map(function(lat, i, arr) {
        return new Array(lat, lngs[i]);
    });
    overlay = L.imageOverlay(imageURL, latLngs).addTo(map);
    map.fitBounds(overlay.getBounds());
}
