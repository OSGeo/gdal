#!/bin/bash
docker build -f ubuntu-small-latest.Dockerfile -t test-ubuntu-small-latest .
docker run -it test-ubuntu-small-latest bash
