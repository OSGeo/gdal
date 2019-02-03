#!/bin/sh

set -e

# Clone proj
git clone --depth=1 https://github.com/OSGeo/proj.4 proj
(cd proj/data && curl http://download.osgeo.org/proj/proj-datumgrid-1.8.tar.gz > proj-datumgrid-1.8.tar.gz && tar xvzf proj-datumgrid-1.8.tar.gz)
 