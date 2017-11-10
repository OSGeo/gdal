import sys
sys.path.insert(0,'/home/ajolma/github/gdal/autotest/pymod/')

try:
    from BaseHTTPServer import HTTPServer
    from BaseHTTPServer import BaseHTTPRequestHandler
except:
    from http.server import BaseHTTPRequestHandler

import urlparse

from osgeo import gdal
    
import webserver

do_log = False

class WCSHTTPHandler(BaseHTTPRequestHandler):

    def log_request(self, code='-', size='-'):
        return

    def Headers(self, type):
        self.send_response(200)
        self.send_header('Content-Type', type)
        self.end_headers()

    def Respond(self, request, brand, version):
        try:
            if request == 'GetCoverage':
                suffix = '.tiff'
            else:
                suffix = '.xml'
            fname = 'responses/' + request + '-' + brand + '-' + version + suffix
            print(fname)
            f = open(fname, 'rb')
            content = f.read()
            f.close()
            self.Headers('application/xml')
            self.wfile.write(content)
        except IOError:
            self.send_error(404, 'File Not Found: ' + request + ' ' + brand + ' ' + version)

    def do_GET(self):
        print(self.path)
        if do_log:
            f = open('/tmp/log.txt', 'a')
            f.write('GET %s\n' % self.path)
            f.close()
        split = urlparse.urlparse(self.path)
        query = urlparse.parse_qs(split.query)
        query2 = {}
        for key in query:
            query2[key.lower()] = query[key]
        self.Respond(query2['request'][0], query2['server'][0], query2['version'][0])
        return

if len(sys.argv) > 1 and sys.argv[1] == "server":
    port = 8080
    server = HTTPServer(('', port), WCSHTTPHandler)
    try:
        print "Starting server"
        server.serve_forever()    
    except KeyboardInterrupt:
        print "Closing server"
        server.server_close()
    sys.exit(0)


(process, port) = webserver.launch(handler = WCSHTTPHandler)

url = "http://127.0.0.1:" + str(port)
try:
    servers = ['MapServer', 'GeoServer', 'Rasdaman', 'ArcGIS']
    versions = ['1.0.0', '1.1.0', '1.1.1', '1.1.2', '2.0.1']
    for server in servers:
        for version in versions:
            if server == 'GeoServer' and version == '1.1.2':
                continue
            if server == 'Rasdaman' and version != '2.0.1':
                continue
            if version == '1.0.0':
                coverages = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                             'GeoServer': 'smartsea:eusm2016-EPSG2393',
                             'ArcGIS': '1'}
            elif version == '1.1.0':
                coverages = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                             'GeoServer': 'smartsea:eusm2016-EPSG2393',
                             'ArcGIS': '1'}
            elif version == '1.1.1':
                coverages = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                             'GeoServer': 'smartsea:eusm2016-EPSG2393',
                             'ArcGIS': '1'}
            elif version == '1.1.2':
                coverages = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                             'GeoServer': 'smartsea:eusm2016-EPSG2393',
                             'ArcGIS': '1'}
            elif version == '2.0.1':
                coverages = {'MapServer': 'BGS_EMODNET_CentralMed-MCol',
                             'GeoServer': 'smartsea__eusm2016-EPSG2393',
                             'Rasdaman': 'BlueMarbleCov',
                             'ArcGIS': 'Coverage1'}
            
            query = 'version=' + version
            query += '&server=' + server
            # get capabilities
            print('test ' + server + ' ' + version )
            ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                             open_options = ['REFRESH_CACHE=TRUE'])
            print(ds);
            query += '&coverage=' + coverages[server]
            ds = gdal.OpenEx(utf8_path = "WCS:" + url + "/?" + query,
                             open_options = ['REFRESH_CACHE=TRUE'])
            print(ds);
            sys.exit(0)
except:
    print "Unexpected error:", sys.exc_info()[0]
    webserver.server_stop(process, port)
    sys.exit(0)

webserver.server_stop(process, port)
