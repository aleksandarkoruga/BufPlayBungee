# BufPlayBungee

Author: Aleksandar Koruga

A SuperCollider UGen for time-stretching and pitch-shifting using the Bungee library.

### Requirements

- CMake >= 3.5
- SuperCollider source code
- Git (for cloning submodules)

### Building

Clone the project with submodules:


    git clone --recursive https://github.com/aleksandarkoruga/bufplaybungee
    cd bufplaybungee
    mkdir build
    cd build

Then, use CMake to configure and build it:

    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
    cmake --build . --config Release --target install

You may want to manually specify the install location in the first step to point it at your
SuperCollider extensions directory: add the option `-DCMAKE_INSTALL_PREFIX=/path/to/extensions`.

It's expected that the SuperCollider repo is cloned at `../supercollider` relative to this repo. If
it's not: add the option `-DSC_PATH=/path/to/sc/source`.

### Windows Notes
When building on Windows with MSVC, the build system automatically:

Adds necessary Windows compatibility headers

Patches Bungee's MSVC compatibility issues

Configures the correct compiler flags

### Developing

Use the command in `regenerate` to update CMakeLists.txt when you add or remove files from the
project. You don't need to run it if you only change the contents of existing files. You may need to
edit the command if you add, remove, or rename plugins, to match the new plugin paths. Run the
script with `--help` to see all available options.
