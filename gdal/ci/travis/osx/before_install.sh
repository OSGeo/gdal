#!/bin/sh

set -e

brew update
brew uninstall postgis gdal
brew install sqlite3 ccache
