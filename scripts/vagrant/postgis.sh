#!/bin/sh

# abort install if any errors occur and enable tracing
set -o errexit
set -o xtrace

sudo sed -i  's/md5/trust/' /etc/postgresql/14/main/pg_hba.conf
sudo sed -i  's/peer/trust/' /etc/postgresql/14/main/pg_hba.conf

sudo service postgresql restart
psql -c "create database autotest" -U postgres
psql -c "create extension postgis" -d autotest -U postgres
