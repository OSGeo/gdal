FROM ubuntu:16.04

RUN apt-get update && apt-get -y install \ 
    build-essential \
    cmake \
    autoconf \
    python2.7 \
    python \
    nodejs \
    default-jre \
    libtool \
    curl \
    git-core \
    zip

WORKDIR /tmp
RUN curl -O https://s3.amazonaws.com/mozilla-games/emscripten/releases/emsdk-portable.tar.gz && \
    tar -xzf emsdk-portable.tar.gz -C /opt && \
    rm emsdk-portable.tar.gz

WORKDIR /opt/emsdk-portable
RUN python2.7 emsdk update && python2.7 emsdk install \
    clang-e1.37.22-64bit \
    node-4.1.1-64bit \
    emscripten-1.37.22 \
    sdk-1.37.22-64bit
RUN python2.7 emsdk activate \
    clang-e1.37.22-64bit \
    node-4.1.1-64bit \
    emscripten-1.37.22 \
    sdk-1.37.22-64bit

# Mimic "source ./emsdk_env.sh"
ENV PATH="/opt/emsdk-portable/emscripten/1.37.22:${PATH}"
ENV PATH="/opt/emsdk-portable/node/4.1.1_64bit/bin:${PATH}"
ENV PATH="/opt/emsdk-portable/clang/e1.37.22_64bit:${PATH}"
ENV PATH="/opt/emsdk-portable:${PATH}"
ENV EMSDK="/opt/emsdk-portable"
ENV BINARYEN_ROOT="/opt/emsdk-portable/clang/e1.37.22_64bit/binaryen"
ENV EMSCRIPTEN="/opt/emsdk-portable/emscripten/1.37.22"

RUN git config --global user.name 'Nobody'
RUN git config --global user.email 'nobody@nowhere.nope'

RUN mkdir /opt/gdaljs
WORKDIR /opt/gdaljs

