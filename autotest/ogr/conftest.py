import pytest
import webserver

from osgeo import ogr

###############################################################################
# Create table from data/poly.shp


@pytest.fixture(scope="module")
def poly_feat():

    with ogr.Open("data/poly.shp") as shp_ds:
        shp_lyr = shp_ds.GetLayer(0)

        return [feat for feat in shp_lyr]


# Start a webserver


@pytest.fixture(scope="module")
def server():

    process, port = webserver.launch(handler=webserver.DispatcherHttpHandler)

    if port == 0:
        pytest.skip()

    import collections

    WebServer = collections.namedtuple("WebServer", "process port")

    yield WebServer(process, port)

    webserver.server_stop(process, port)


@pytest.fixture()
def file_handler():

    handler = webserver.FileHandler({})

    with webserver.install_http_handler(handler):
        yield handler


@pytest.fixture()
def handle_get(file_handler):
    return file_handler.handle_get


@pytest.fixture()
def handle_delete(file_handler):
    return file_handler.handle_delete


@pytest.fixture()
def handle_put(file_handler):
    return file_handler.handle_put


@pytest.fixture()
def handle_post(file_handler):
    return file_handler.handle_post
