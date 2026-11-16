#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake -S "$project_dir" -B "$project_dir/build-host" -G "Unix Makefiles"
cmake --build "$project_dir/build-host" --parallel
ctest --test-dir "$project_dir/build-host" --output-on-failure
"$project_dir/tools/build_firmware.sh"

