#!/bin/bash
docker build -f gitpod-workspace-full.Dockerfile -t test-gitpod-workspace-full .
echo "Entering test docker container. When inspection completed 'exit' to return."
docker run -it test-gitpod-workspace-full bash
