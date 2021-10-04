#!/bin/bash
set -x -e
source /scripts/.venv/bin/activate
export CC=`which gcc-10`
export CXX=`which gxx-10`
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make server
