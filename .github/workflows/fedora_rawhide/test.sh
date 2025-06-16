#!/bin/bash

set -e

# Do not run in parallel to avoid issues with local webserver
ctest -V
