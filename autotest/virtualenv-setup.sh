#!/bin/bash
set -eu

# install pip
curl https://bootstrap.pypa.io/get-pip.py | sudo -H python

# install virtualenv
sudo -H pip install virtualenv

virtualenv ~/venv
rm -f ~/venv/lib/python2.7/no-global-site-packages.txt

~/venv/bin/pip install -Ur /vagrant/autotest/requirements.txt

# Set things up for gdb: https://stackoverflow.com/a/30430059/988527
sudo apt-get install -y python2.7-dbg
mkdir ~/venv/bin/.debug
ln -s /usr/lib/debug/usr/bin/python2.7-gdb.py ~/venv/bin/.debug/python-gdb.py
ln -s /usr/lib/debug/usr/bin/python2.7 ~/venv/bin/.debug/

echo "

created virtualenv in ~/venv. To activate it:
    source ~/venv/bin/activate
"
