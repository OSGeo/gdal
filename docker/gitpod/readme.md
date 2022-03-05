# Gitpod configuration files

**[Gitpod](https://www.gitpod.io/)** is a clould IDE environment for automatically spinning up fresh linux dev environments straight from a git repository and developing in your browser. This directory contains Gitpod configuration files for running gdal docker images in Gitpod.

**Why?** The initiating spark was having an easily reproducible and complete GDAL linux machine available to test changes being made to python scripts by a Windows developer. Much more than this simple goal is possible from this base point though.

Generally what the files do is:

- Load a published OSGeo GDAL docker image
- install or update dependencies with apt-get
- Install `sudo` and add the *gitpod* user to it's list
- install python development libraries and modules

## Files

`/.gitpod.yml` - Pointer to the Dockerfile containing our settings.

`./docker/gitpod/osgeo-gdal.Dockerfile` - a [custom Gitpod dockerfile](https://www.gitpod.io/docs/config-docker#configure-a-custom-dockerfile) that uses the **osgeo-gdal** image on Docker Hub as a starting point.

`./docker/gitpod/ubuntu-small-latest.Dockerfile` - as above but using the slim downed ubuntu image

`./docler/gitpod/gitpod-workspace-full`  - loads the default workspace in Gitpod with all bells and whistles, then checks out current gdal sources and builds them. This takes a long time. *And is currently broken.*

`test-$IMAGE-NAME.sh` - run this from a default gitpod image to test the associated .Dockerfile contents without having to commit every change to repo first (see [Trying out changes to your Dockerfile](https://www.gitpod.io/docs/config-docker#trying-out-changes-to-your-dockerfile)).

## Developing

- Fire up gitpod or other docker enabled machine
- Edit .Dockerfile as desired
- Test the docker file validity with
  `docker build -f osgeo-geo.Dockerfile -t test-osgeo-gdal .`
  `docker run -it test-gitpod-workspace-full bash`
- Commit changes
- Run `https://gitpod.io/#URL_OF_YOUR_FORK`, e.g.
  https://gitpod.io/#https://github.com/maphew/gdal/tree/gitpod/

## Known limitations

Alpine images aren't used due to an upstream incompatibilty in Gitpod ([Issue #3356](https://github.com/gitpod-io/gitpod/issues/3356)).

A separate git branch is needed for each Gitpod image, due to hardcoding of Dockerfile path in `.gitpod.yml`.

## Future Work

This implementation uses GDAL images that are published to Docker Hub, so they are always somewhat older than current development. It would be good to adapt any or all of `./docker/*/Dockerfile` so that gitpod can be launched from current state of the repository.
