var boundsMapper = new Worker('worker.js');

var map = L.map('leaflet').setView([0,0],3);

L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="http://osm.org/copyright">OpenStreetMap</a> contributors',
    maxZoom: 18,
}).addTo(map);

var feature = null;

function mapBounds() {
    var files = document.querySelector('#geotiff-select').files;
    boundsMapper.postMessage(files);
}

boundsMapper.onmessage = function(evt) {
    // Coordinates are coming back as two arrays, [lons, lats]
    if (feature !== null) {
        feature.remove();
    }
    
    var lats = Array.from(evt.data[1]);
    var lngs = Array.from(evt.data[0]);

    // Zip
    var latLngs = lats.map(function(lat, i, arr) {
        return new Array(lat, lngs[i]);
    });
    feature = L.polygon(latLngs).addTo(map);
    map.fitBounds(feature.getBounds());
};
