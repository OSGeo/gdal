#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit
set -o xtrace

wget -c -nc -nv -P /var/cache/wget https://github.com/uclouvain/openjpeg/releases/download/v2.3.0/openjpeg-v2.3.0-linux-x86_64.tar.gz
tar xzf /var/cache/wget/openjpeg-v2.3.0-linux-x86_64.tar.gz
sudo cp -r openjpeg-v2.3.0-linux-x86_64/include/* /usr/local/include
sudo cp -r openjpeg-v2.3.0-linux-x86_64/lib/* /usr/local/lib

