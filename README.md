# CRADLE

[![MSVC](https://github.com/mghro/cradle/actions/workflows/msvc.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/msvc.yml)
[![GCC](https://github.com/mghro/cradle/actions/workflows/gcc.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/gcc.yml)
[![Docker](https://github.com/mghro/cradle/actions/workflows/docker.yml/badge.svg)](https://github.com/mghro/cradle/actions/workflows/docker.yml)
[![Code Coverage](https://codecov.io/gh/mghro/cradle/branch/main/graph/badge.svg)](https://codecov.io/gh/mghro/cradle)

CRADLE acts as a local proxy for Thinknode

## Build Requirements

### Supported OSs and C++ Compilers

The following platforms/compilers are supported for building CRADLE:

- Ubuntu 20.04 (Focal), using GCC 10 or higher
- Windows 10, using VS 2019 or higher

Other platforms (e.g., other Linux distros) will likely work with some
adaptation.

CRADLE makes use of some C++20 features, so other compilers (or earlier
versions of the supported compilers) likely won't work. It is a goal though to
support Clang once its C++20 support matures.

### CMake

CRADLE is built using CMake and requires a relatively recent version. It's
known to work on 3.18 and 3.19. It should work on more recent versions as well.

### OCaml

CRADLE has a custom preprocessor that's written in OCaml, so the build process
currently require OCaml to be installed. It can be installed available via a
package manager.

#### Ubuntu

```shell
sudo apt-get install -y ocaml-nox
```

#### Windows (via Chocolatey)

```shell
choco install ocpwin
```

### Python

CRADLE uses Python (3.x) both indirectly (via Conan) and directly (for some
testing purposes).

#### Ubuntu

Python itself should be available by default. If necessary, install the
`virtualenv` tool as follows:

```shell
sudo pip3 install virtualenv
```

You can then use the provided CRADLE script to set up a virtual environment and
`source` it:

```shell
scripts/set-up-python.sh --python=python3
source .venv/bin/activate
```

#### Windows

Install Python 3.x however you prefer, e.g.:

```shell
choco install python3
```

No script is currently provided to set up a virtual environment on Windows, but
you can do so yourself if you like.

Either way, you should install the following packages:

```shell
pip install conan gcovr pytest websocket-client msgpack
```

## Secrets

A couple of Thinknode secrets are currently required to build and test CRADLE.
Be sure to acquire them. You have two options for passing them into the build
system:

You can store them in environment variables:

- `CRADLE_THINKNODE_TOKEN`
- `CRADLE_THINKNODE_DOCKER_AUTH`

Or, alternatively, you can store them in files within the root `cradle`
directory:

- `.token`
- `.docker-auth`

(Each file should contain just the secret string.)

## Building

Once you have everything set up, actually building follows the standard CMake
form:

```shell
cmake -Bbuild .
```

or

```shell
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release .
```

You should also be able to use the CMake integration tools in Visual Studio
Code (tested) or Visual Studio (untested).

## Testing

You can build/run the unit tests using the `unit_tests` CMake target, either by
selecting it in your IDE or invoking it with CMake:

```shell
cmake --build build --config Release --target unit_tests
```

(There is also an `integration_tests` target, but that is a bit outdated at the
moment, and it's not really clear that the current form has any purpose in the
project moving forward.)
