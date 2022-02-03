Example Development Environment
===============================

An example of setting up a development environment for *gdal-utils* using Anaconda.

In Windows CMD shell::
    pushd path\to\my\code-workspaces

    :: Create dev enironment and install gdal & python
    conda create -n gdal
    conda activate gdal
    conda install gdal

    :: Get the current gdal code
    :: We assume you've forked the gdal repository on GitHub under your username.
    set GITHUB_USERNAME=my_username
    git clone --depth=50 https://%GITHUB_USERNAME%@github.com/%GITHUB_USERNAME%/gdal.git

    :: Create the local branch you'll be developing in
    git branch gdal-utils-mine
    git checkout gdal-utils-mine

    :: **Important** This is the gdal-utils project root. Ignore all parent folders.
    pushd gdal\swig\python\gdal-utils

    :: Replace the gdal-utils installed via conda with our dev environment version
    pip install --editable .

    :: Start hacking! (Launch VS-Code and open current dir)
    code .
