#!/bin/sh

set -e

# Clone proj
#git clone --depth=1 https://github.com/OSGeo/proj.4 proj
curl http://download.osgeo.org/proj/proj-6.0.0.tar.gz > proj-6.0.0.tar.gz
tar xzf proj-6.0.0.tar.gz
mv proj-6.0.0 proj
echo "#!/bin/sh" > proj/autogen.sh
chmod +x proj/autogen.sh
(cd proj/data && curl http://download.osgeo.org/proj/proj-datumgrid-1.8.tar.gz > proj-datumgrid-1.8.tar.gz && tar xvzf proj-datumgrid-1.8.tar.gz)
 