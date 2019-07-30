#!/bin/sh

set -e

brew update
if brew ls --versions postgis >/dev/null
then
	brew uninstall postgis
fi
if brew ls --versions gdal >/dev/null
then
	brew uninstall gdal
fi
brew install sqlite3 ccache
brew upgrade sqlite
