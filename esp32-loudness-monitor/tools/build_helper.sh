#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$project_dir" -B "$project_dir/build-helper" -G "Unix Makefiles" \
  -DLMV1_BUILD_HELPER=ON -DLMV1_BUILD_TESTS=OFF
cmake --build "$project_dir/build-helper" --parallel

