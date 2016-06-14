#!/bin/sh

# abort install if any errors occur and enable tracing
set -o errexit
set -o xtrace

sudo sed -i  's/md5/trust/' /etc/postgresql/9.1/main/pg_hba.conf
sudo sed -i  's/peer/trust/' /etc/postgresql/9.1/main/pg_hba.conf

sudo ln -s /usr/lib/libgdal.so.2 /usr/lib/libgdal.so.1
sudo service postgresql restart
psql -c "create database autotest" -U postgres
psql -c "create extension postgis" -d autotest -U postgres
