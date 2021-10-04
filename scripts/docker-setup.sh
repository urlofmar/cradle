#!/bin/bash
set -x -e
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y lcov ocaml-nox python3-pip cmake git gcc-10
pip3 install virtualenv
gcc-10 --version
./set-up-python.sh --python=python3
