#!/bin/bash
# Set up a Python virtual environment so that it can build CRADLE.
echo "Setting up Python environment in .venv..."
set -x -e
virtualenv "$@" --prompt="(cradle) " --python=python3 .venv
source .venv/bin/activate
python --version
pip install conan jinja2 gcovr pytest websocket-client msgpack
