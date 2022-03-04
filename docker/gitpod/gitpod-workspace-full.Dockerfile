FROM gitpod/workspace-full
RUN uname -a && cat /etc/os-release

# Get gdal sources, limit to 50 most recent commits, with branches, and checkout master
RUN git clone --depth=50 https://github.com/osgeo/gdal.git \
    && cd gdal \
    && git remote set-branches origin "*" \
    && git fetch -v --depth=50 \
    && git checkout master

# install gdal dependencies
RUN sudo bash gdal/.github/workflows/ubuntu_20.04/build-deps.sh
# build gdal
RUN bash /workspace/gdal/.github/workflows/ubuntu_20.04/build.sh



## --- Scrapbook ---
# # RUN apt-get install -y software-properties-common
# # RUN add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
#     # results in "E: Unable to fetch some archives, maybe run apt-get update or try with --fix-missing?"
# RUN apt-get update --fix-missing
# RUN apt-get install -y apt-utils

# ### Gitpod user ###
# # https://community.gitpod.io/t/how-to-resolve-password-issue-in-sudo-mode-for-custom-image/2395/3
# # '-l': see https://docs.docker.com/develop/develop-images/dockerfile_best-practices/#user
# RUN apt-get install -y sudo
# RUN useradd -l -u 33333 -G sudo -md /home/gitpod -s /bin/bash -p gitpod gitpod \
#     # passwordless sudo for users in the 'sudo' group
#     && sed -i.bkp -e 's/%sudo\s\+ALL=(ALL\(:ALL\)\?)\s\+ALL/%sudo ALL=NOPASSWD:ALL/g' /etc/sudoers

# # Dependencies for python development
# RUN apt-get install -y python3-dev libpython3-dev libpython3.8-dev python3.8-dev python3-numpy \
#     python3-lxml pyflakes python3-setuptools python3-pip python3-venv

# # These might be better executed when needed, in a shell session?
# #RUN python3 -m pip install -U pip wheel setuptools numpy
# # Enable for testing
# #RUN python3 -m pip install -r /workspace/gdal/autotest/requirements.txt

# ENV PATH=/home/gitpod/.local/bin:$PATH
# ENV CMAKE_BUILD_TYPE=release
