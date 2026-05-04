# Inspired from https://github.com/anaconda/docker-images/blob/main/miniconda3/debian/Dockerfile

FROM debian:13.4-slim@sha256:cedb1ef40439206b673ee8b33a46a03a0c9fa90bf3732f54704f99cb061d2c5a

ENV LANG=C.UTF-8 LC_ALL=C.UTF-8

RUN apt-get update -q && \
    apt-get install -q -y --no-install-recommends \
        bzip2 \
        ca-certificates \
        procps \
        wget \
        curl \
        unzip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ENV PATH=/opt/conda/bin:$PATH

CMD [ "/bin/bash" ]

# Leave these args here to better use the Docker build cache
# renovate: datasource=custom.miniconda_installer
ARG INSTALLER_URL_LINUX64="https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-x86_64.sh"
# renovate: datasource=custom.miniconda_installer
ARG INSTALLER_URL_AARCH64="https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh"

RUN set -x && \
    UNAME_M="$(uname -m)" && \
    if [ "${UNAME_M}" = "x86_64" ]; then \
        INSTALLER_URL="${INSTALLER_URL_LINUX64}"; \
    elif [ "${UNAME_M}" = "aarch64" ]; then \
        INSTALLER_URL="${INSTALLER_URL_AARCH64}"; \
    fi && \
    wget "${INSTALLER_URL}" -O miniforge3.sh -q && \
    mkdir -p /opt && \
    bash miniforge3.sh -b -p /opt/conda && \
    rm miniforge3.sh && \
    ln -s /opt/conda/etc/profile.d/conda.sh /etc/profile.d/conda.sh && \
    echo ". /opt/conda/etc/profile.d/conda.sh" >> ~/.bashrc && \
    echo "conda activate" >> ~/.bashrc && \
    /opt/conda/bin/conda install python==3.14 && \
    find /opt/conda/ -follow -type f -name '*.a' -delete && \
    find /opt/conda/ -follow -type f -name '*.js.map' -delete && \
    /opt/conda/bin/conda clean -afy

RUN set -x && \
    /opt/conda/bin/conda install gdal libgdal yq qgis && \
    find /opt/conda/ -follow -type f -name '*.a' -delete && \
    find /opt/conda/ -follow -type f -name '*.js.map' -delete && \
    /opt/conda/bin/conda clean -afy

RUN set -x && \
    mkdir /data && \
    cd /data && \
    wget -q https://github.com/rouault/gdal_cli_workshop_data/archive/refs/heads/master.zip && \
    unzip master.zip && \
    rm -f master.zip

# For QGIS
RUN echo "export DISPLAY=:1" >> ~/.bashrc && \
    echo "export QT_XCB_GL_INTEGRATION=none" >> ~/.bashrc
