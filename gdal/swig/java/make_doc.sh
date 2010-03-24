#!/bin/sh

# Patch the generated SIWG Java files to add the Javadoc into them
# thanks to the small utility add_javadoc
rm -rf org_patched
mkdir org_patched
cp -r org org_patched
echo "Patching .java files with Javadoc from javadoc.java"
gcc -g -Wall add_javadoc.c -o add_javadoc
./add_javadoc javadoc.java org_patched `find org -name "*.java"`

# Generate the HTML Javadoc
rm -rf java
cp gdal-package-info.java org_patched/org/gdal/gdal/package-info.java
cp gdalconst-package-info.java org_patched/org/gdal/gdalconst/package-info.java
cp ogr-package-info.java org_patched/org/gdal/ogr/package-info.java
cp osr-package-info.java org_patched/org/gdal/osr/package-info.java
javadoc -overview overview.html -public -d ./java -sourcepath org_patched -subpackages org.gdal -link http://java.sun.com/javase/6/docs/api -windowtitle "GDAL/OGR 1.8.0 Java bindings API"

# Create a zip with the Javadoc
rm -f javadoc.zip
zip -r javadoc.zip java
