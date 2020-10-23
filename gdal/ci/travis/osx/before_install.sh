#!/bin/sh

set -e

# Temp hack indicated in https://github.com/actions/virtual-environments/issues/1811#issuecomment-708480190
brew uninstall openssl@1.0.2t
brew uninstall python@2.7.17
brew untap local/openssl
brew untap local/python2
brew cask install xquartz

brew update
sudo -H pip3 install -U -r autotest/requirements.txt

if brew ls --versions postgis >/dev/null
then
	brew uninstall postgis
fi
if brew ls --versions gdal >/dev/null
then
	brew uninstall gdal
fi
brew install ccache
