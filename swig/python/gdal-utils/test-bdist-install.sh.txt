#!/usr/bin/env bash
echo "--- Creating a binary wheel and installing it virtual env..."
python setup.py bdist_wheel

python -m venv test-wheel
cd test-wheel
source ./bin/activate
python3 -m pip install -U pip wheel setuptools numpy
pip install ../dist/gdal_utils-*.whl

echo "--- Keeping shell open so the venv can be explored (verify with 'which python')."
echo "--- Next steps: verify the installed gdal_edit etc. scripts work as expected."
exec bash -i
