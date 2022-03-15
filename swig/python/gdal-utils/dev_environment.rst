Example Development Environment
===============================

An example of setting up a development environment for *gdal-utils* using Anaconda.

In Windows CMD shell::
    pushd path\to\my\code-workspaces

    :: Create dev environment and install gdal & python
    conda create -n gdal
    conda activate gdal]

    # ensure to get community maintained version via conda-forge
    conda install -c conda-forge gdal

    :: Get the current gdal code (limited to most recent 50 commits)
    :: We assume you've forked the gdal repository on GitHub under your username.
    set GITHUB_USERNAME=my_username
    git clone --depth=50 https://%GITHUB_USERNAME%@github.com/%GITHUB_USERNAME%/gdal.git

    :: Allow checking out of remote branches
    git remote set-branches origin "*"
    git fetch -v --depth=50

    :: Create the local branch you'll be developing in
    git branch gdal-utils-mine
    git checkout gdal-utils-mine

    :: **Important** This is the gdal-utils project root. Ignore all parent folders.
    pushd gdal\swig\python\gdal-utils

    :: Replace the gdal-utils installed via conda with our dev environment version
    pip install --editable .

    :: Start hacking! (Launch VS-Code with current dir. Adjust for your IDE)
    code .
