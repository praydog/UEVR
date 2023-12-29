# Compiling UEVR

## Necessary prerequisites

Your GitHub account must have access to the [EpicGames](https://github.com/EpicGames/) GitHub organization. If you do not have access, you will not be able to compile UEVR.

This is because the [UESDK](https://github.com/praydog/UESDK) submodule is a fork of the [EpicGames/UnrealEngine](https://github.com/EpicGames/UnrealEngine) repository. You must have an SSH key set up with your GitHub account and an SSH agent running in order to clone the UESDK submodule.

A C++23 compatible compiler is required. Visual Studio 2022 is recommended. Compilers other than MSVC have not been tested.

CMake is required.

## Compiling

###  Clone the repository

#### SSH
```
git clone git@github.com:praydog/UEVR.git
```

#### HTTPS
```
git clone https://github.com/praydog/UEVR
```

### Initialize the submodules

```
git submodule update --init --recursive
```

### Set up CMake

#### Command line

```
cmake -S . -B build ./build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build ./build --config Release --target uevr
```

#### VSCode

1. Install the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension
2. Open the UEVR folder in VSCode
3. Press `Ctrl+Shift+P` and select `CMake: Configure`
4. When "Select a kit" appears, select `Visual Studio Community 2022 Release - amd64`
5. Select the desired build config (usually `Release` or `RelWithDebInfo`)
6. You should now be able to compile UEVR by pressing `Ctrl+Shift+P` and selecting `CMake: Build` or by pressing `F7`