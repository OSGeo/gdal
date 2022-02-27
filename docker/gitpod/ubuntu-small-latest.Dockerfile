FROM osgeo/gdal:ubuntu-small-latest
RUN uname -a && cat /etc/os-release
# RUN apt-get install -y software-properties-common
# RUN add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
    # results in "E: Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?"
RUN apt-get update --fix-missing
RUN apt-get install -y apt-utils

### Gitpod user ###
# https://community.gitpod.io/t/how-to-resolve-password-issue-in-sudo-mode-for-custom-image/2395/3
# '-l': see https://docs.docker.com/develop/develop-images/dockerfile_best-practices/#user
RUN apt-get install -y sudo
RUN useradd -l -u 33333 -G sudo -md /home/gitpod -s /bin/bash -p gitpod gitpod \
    # passwordless sudo for users in the 'sudo' group
    && sed -i.bkp -e 's/%sudo\s\+ALL=(ALL\(:ALL\)\?)\s\+ALL/%sudo ALL=NOPASSWD:ALL/g' /etc/sudoers

# full list dependencies according to
# https://github.com/OSGeo/gdal/blob/6e6aff451dbcde450f051bff2f2e75ce6a4a3e6f/.github/workflows/cmake_builds.yml#L37
RUN apt-get install -y bison libjpeg-dev libgif-dev liblzma-dev libzstd-dev libgeos-dev git \
   libcurl4-gnutls-dev libproj-dev libxml2-dev  libxerces-c-dev libnetcdf-dev netcdf-bin \
   libpoppler-dev libpoppler-private-dev gpsbabel libhdf4-alt-dev libhdf5-serial-dev libpodofo-dev poppler-utils \
   libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libcrypto++-dev libdap-dev libkml-dev \
   libmysqlclient-dev libarmadillo-dev wget libfyba-dev libjsoncpp-dev libexpat1-dev \
   libclc-dev ocl-icd-opencl-dev libsqlite3-dev sqlite3-pcre libpcre3-dev libspatialite-dev libsfcgal-dev fossil libcairo2-dev libjson-c-dev libdeflate-dev liblz4-dev libblosc-dev \
   libqhull-dev libcfitsio-dev libogdi-dev libopenjp2-7-dev libcharls-dev libheif-dev \
   python3-dev libpython3-dev libpython3.8-dev python3.8-dev python3-numpy python3-lxml pyflakes python3-setuptools python3-pip python3-venv \
   python3-pytest swig doxygen texlive-latex-base make cppcheck ccache g++ \
   libpq-dev libpqtypes-dev postgresql-12 postgresql-12-postgis-3 postgresql-client-12 postgresql-12-postgis-3-scripts

RUN python3 -m pip install -U pip wheel setuptools numpy
# Enable for testing
#RUN python3 -m pip install -r /workspace/gdal/autotest/requirements.txt

ENV PATH=/home/gitpod/.local/bin:$PATH
ENV CMAKE_BUILD_TYPE=release
