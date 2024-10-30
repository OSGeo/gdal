#!/bin/sh

rsync -Paz --exclude .git --exclude __pycache__ --exclude 'autotest/gdrivers/tmp/cache' --exclude 'autotest/pyscripts/out*' --exclude 'autotest/gcore/tmp' --exclude 'autotest/ogr/tmp' --exclude 'autotest/utilities/tmp' --exclude 'build-*' --exclude '*~' ~/src/gdal indigo:src
