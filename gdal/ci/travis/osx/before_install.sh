#!/bin/sh

set -e

brew update
brew uninstall gdal
brew install sqlite3
brew install ccache
