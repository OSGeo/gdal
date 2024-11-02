#!/bin/bash

set -eu

BENCHMARK_STORAGE="file://${PWD}/benchmark_results"

# Use time.process_time for more reliability on VMs
BENCHMARK_OPTIONS=(
        "--benchmark-storage=${BENCHMARK_STORAGE}" \
        "--benchmark-timer=time.process_time" \
)

# Dry run to hopefully stabilize later timings
(cd old_version/gdal/build; source ../scripts/setdevenv.sh; pytest autotest/benchmark "${BENCHMARK_OPTIONS[@]}" --capture=no -ra -vv)

# Run reference (old) build and save its results
(cd old_version/gdal/build; source ../scripts/setdevenv.sh; pytest autotest/benchmark "${BENCHMARK_OPTIONS[@]}" --benchmark-save=ref  --capture=no -ra -vv)

# Run target build and compare its results to the reference one.
# Fail if we get results 20% slower or more.
# Retry if that fails a first time.
BENCHMARK_COMPARE_OPTIONS=(
        "--benchmark-compare-fail=min:20%" \
        "--benchmark-compare=0001_ref" \
)

(source ${GDAL_SOURCE_DIR:=..}/scripts/setdevenv.sh; pytest autotest/benchmark "${BENCHMARK_OPTIONS[@]}" "${BENCHMARK_COMPARE_OPTIONS[@]}" --capture=no -ra -vv || (echo "Retrying..."; pytest autotest/benchmark "${BENCHMARK_OPTIONS[@]}" "${BENCHMARK_COMPARE_OPTIONS[@]}" --capture=no -ra -vv))
