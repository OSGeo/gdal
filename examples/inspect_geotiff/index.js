var tiffInspector = new Worker('worker.js');

function inspectFiles() {
    var files = document.querySelector('#geotiff-select').files;
    tiffInspector.postMessage(files);
}

tiffInspector.onmessage = function(evt) {
    console.log(evt.data);
};
