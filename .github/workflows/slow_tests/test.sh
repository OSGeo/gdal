#!/bin/bash

set -e

. ../scripts/setdevenv.sh

(cd autotest && ./run_slow_tests.sh)

