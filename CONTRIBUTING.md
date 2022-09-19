# Setting development environment (Unix)

## Build SWIG

SWIG (https://swig.org/) is required to compile GDAL with other programming languages, such as Python.

> **Note**
> SWIG version 4.0.2 is required.

Set the installation path:

```bash
export SWIG_PREFIX=/path/to/install

```

Install SWIG from source:

```bash
export SWIG_VERSION=4.0.2
mkdir /tmp/swig/
cd /tmp/swig/
wget https://sourceforge.net/projects/swig/files/swig/swig-${SWIG_VERSION}/swig-${SWIG_VERSION}.tar.gz/download -O swig-${SWIG_VERSION}.tar.gz
tar xf swig-${SWIG_VERSION}.tar.gz
cd swig-${SWIG_VERSION}
./configure --prefix=$SWIG_PREFIX
make
make install
export PATH=$SWIG_PREFIX/bin:$PATH
```

## Build GDAL: CMake

Install all required development packages: GNU make, g++, ...

Setup [Python Virtual environment](https://docs.python.org/3/library/venv.html):

> **Note**
> Add the [Python_FIND_VIRTUALENV=ONLY](https://gdal.org/build_hints.html#cmdoption-arg-Python_FIND_VIRTUALENV) `cmake` option with a Python virtual environment.

```bash
python -m venv gdal_venv
. gdal_venv/bin/activate
python -m pip install numpy
```

Setup pre-commit:

```bash
python -m pip install pre-commit
pre-commit install
```


Configure and build:

> **Note**
> For a minimal build, add these options: `GDAL_BUILD_OPTIONAL_DRIVERS=OFF` `OGR_BUILD_OPTIONAL_DRIVERS=OFF`.
> To enable specific drivers, add `GDAL_ENABLE_DRIVER_<driver_name>=ON` or `OGR_ENABLE_DRIVER_<driver_name>=ON`.
> See [Selection of Drivers](https://gdal.org/build_hints.html#selection-of-drivers)
> for more details.

```bash
mkdir build
cd build
cmake .. -DSWIG_EXECUTABLE=$SWIG_PREFIX/bin/swig -DSWIG_REGENERATE_PYTHON=ON [options]
cmake --build . -j$(nproc)
```

Run command line utilities (without installing):
```bash
. ../scripts/setdevenv.sh
gdalinfo --version
```
This will set the PATH, LD_LIBRARY_PATH/DY_LD_LIBRARY_PATH, GDAL_DATA and PYTHONPATH environment variables to point to the build artifacts.

Run autotest suite:
```bash
python -m pip install -r ../autotest/requirements.txt
pytest autotest
```

## Build GDAL: use Vagrant

Make sure the Vagrant binary is installed on your system.

Perform initial setup of the Vagrant GDAL virtual machine:
```bash
# VAGRANT_VM_CPU=number_of_cpus
vagrant up
```

And then to incrementally develop into it:
```bash
vagrant ssh
ninja
```

Note that the following directories on the host will be created (and can be
removed if no longer needed the Vagrant environment):
- ../apt-cache/ubuntu/jammy64: contains a cache of Ubuntu packages of the VM,
  to allow faster VM reconstruction
- build_vagrant: CMake build directory
- ccache_vagrant: CCache directory

## Build GDAL: Autotools

> **Note**
> Only applies to GDAL 3.4 or earlier

Install all required development packages: GNU make, g++, ...

Build:

```bash
./autogen.sh
./configure --with-python [other options]
make -j8 -s
cd apps; make -s test_ogrsf; cd ..
```

Run command line utilities (without installing):
```bash
. scripts/setdevenv.sh
gdalinfo --version
```

Run autotest suite:
```bash
cd autotest
pip install -r requirements.txt
pytest
```

# Git workflows with GDAL

This is not a git tutorial or reference manual by any means. This just collects
a few best practice for git usage for GDAL development.

## Commit message

Indicate a component name (eg a driver name), a short description and when
relevant, a reference to a issue (with 'fixes #' if it actually fixes it)

```
COMPONENT_NAME: fix bla bla (fixes #1234)

Details here...
```

## Initiate your work repository

Fork OSGeo/gdal from github UI, and then
```bash
git clone https://github.com/OSGeo/gdal
cd gdal
git remote add my_user_name git@github.com:my_user_name/gdal.git
```

## Updating your local master against upstream master

```bash
git checkout master
git fetch origin
# Be careful: this will loose all local changes you might have done now
git reset --hard origin/master
```

## Working with a feature branch

```bash
git checkout master
# potentially update your local master against upstream, as described above
git checkout -b my_new_feature_branch

# do work. For example:
git add my_new_file
git add my_modifid_message
git rm old_file
git commit -a

# you may need to resynchronize against master if you need some bugfix
# or new capability that has been added since you created your branch
git fetch origin
git rebase origin/master

# At end of your work, make sure history is reasonable by folding non
# significant commits into a consistent set
git rebase -i master
# use 'fixup' for example to merge several commits together,
# and 'reword' to modify commit messages

# or alternatively, in case there is a big number of commits and marking
# all them as 'fixup' is tedious
git fetch origin
git rebase origin/master
git reset --soft origin/master
git commit -a -m "Put here the synthetic commit message"

# push your branch
git push my_user_name my_new_feature_branch
From GitHub UI, issue a pull request
```

If the pull request discussion or Travis-CI/AppVeyor checks require changes,
commit locally and push. To get a reasonable history, you may need to
```git rebase -i master```, in which case you will have to force-push your
branch with ```git push -f my_user_name my_new_feature_branch```


## Backporting bugfixes from master to a stable branch

```bash
git checkout master
With git log, identify the sha1sum of the commit you want to backport
git checkout 2.2 # if you want to backport to 2.2
git pull origin 2.2
# git checkout -b branch_name  # if you intend to submit the backport as a pull request
git cherry-pick the_sha1_sum
git push ...
```
If changes are needed, do them and ```git commit -a --amend```


## Things you should NOT do

(For anyone with push rights to github.com/OSGeo/gdal) Never modify a commit or
the history of anything that has been
committed to https://github.com/OSGeo/gdal

Committing symbolic links is allowed only under the .github directory in order to
avoid potential problems on Windows.
