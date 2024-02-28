# meshconv
Convert 3D models to a useable form for a game engine. Converts from (.glb/gltf) files to .mesh and .level files

# Installing
Pull the repo, making sure to get the submodules as well
```
git clone --recursive https://github.com/bitGraf/meshconv.git
```
Create a build directory, and generate CMake files
```
mkdir build
cd build
cmake ..
```

Either open the generated project or build via the command line.
```
cmake --build .
```

# Usage
```
meshconv [-v | --version] [-h | --help] <mode> input_filename [-o path/to/output] [-flip-uv]
```
### Mesh mode
```
meshconv mesh input.gltf -o path/to/output -flip-uv
```

# File Formats
010 templates can be found in /templates/