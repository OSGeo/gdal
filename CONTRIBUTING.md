Setting development environment (Unix)
======================================

Install all required development packages: GNU make, g++, ...

Build:

```
./autogen.sh
./configure --with-python [other options]
make -j8 -s
cd apps; make -s test_ogrsf; cd ..
```

Run command line utilities (without installing):
```
. scripts/setdevenv.sh
gdalinfo --version
```

Run autotest suite:
```
cd autotest
pip install -r requirements.txt
pytest
```

Setting development environment (CMake, EXPERIMENTAL)
=====================================================

Note: CMake builds are EXPERIMENTAL for now.

Install all required development packages: GNU make, g++, ...

Configure and build:

```
mkdir build
cd build
cmake .. [options]
make -j8 -s
```

Run command line utilities (without installing):
```
. ../scripts/setdevenv.sh
gdalinfo --version
```

Run autotest suite:
```
pip install -r ../autotest/requirements.txt
pytest autotest
```

Git workflows with GDAL
=======================

This is not a git tutorial or reference manual by any means. This just collects
a few best practice for git usage for GDAL development.

Commit message
--------------

Indicate a component name (eg a driver name), a short description and when
relevant, a reference to a issue (with 'fixes #' if it actually fixes it)

```
COMPONENT_NAME: fix bla bla (fixes #1234)

Details here...
```

Initiate your work repository
-----------------------------

Fork OSGeo/gdal from github UI, and then
```
git clone https://github.com/OSGeo/gdal
cd gdal
git remote add my_user_name https://github.com/my_user_name/gdal.git
```

Updating your local master against upstream master
--------------------------------------------------

```
git checkout master
git fetch origin
# Be careful: this will loose all local changes you might have done now
git reset --hard origin/master
```

Working with a feature branch
-----------------------------

```
git checkout master
(potentially update your local master against upstream, as described above)
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
git rebase -i master (use 'fixup' for example to merge several commits together,
and 'reword' to modify commit messages)

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


Backporting bugfixes from master to a stable branch
---------------------------------------------------

```
git checkout master
With git log, identify the sha1sum of the commit you want to backport
git checkout 2.2 (if you want to backport to 2.2)
git pull origin 2.2
(git checkout -b branch_name: if you intend to submit the backport as a pull request)
git cherry-pick the_sha1_sum
git push ...
```
If changes are needed, do them and ```git commit -a --amend```


Things you should NOT do
------------------------

(For anyone with push rights to github.com/OSGeo/gdal) Never modify a commit or
the history of anything that has been
committed to https://github.com/OSGeo/gdal

Committing symbolic links is allowed only under the .github directory in order to
avoid potential problems on Windows.
