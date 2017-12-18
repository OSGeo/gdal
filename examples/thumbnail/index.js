var thumbnailer = new Worker('worker.js');

function makeThumbnail() {
    var files = document.querySelector('#geotiff-select').files;
    thumbnailer.postMessage(files);
}

thumbnailer.onmessage = function(evt) {
    displayImage(evt.data);
};

function displayImage(imageBytes) {
    var outputBlob = new Blob([imageBytes], { type: 'image/png' });
    var imgDisplay = document.querySelector('#thumbnail');
    imgDisplay.src = window.URL.createObjectURL(outputBlob);
}
