# mesh2audio

Generate axisymmetric 3D models, or import existing 3D models, and transform them into real-time playable physical audio models!

Supports fast DSP generation of physical audio models that sound decently realistic, as well as blazing fast 2D axisymmetric model generation, at the expense of some fidelity.

The generated audio model can be played in real-time by "striking" (clicking) on mesh vertices in the 3D mesh viewer, or by using an audio input device (such as a microphone) to excite the vertices.

* Project video: https://youtu.be/RwxgOHVBDvc
* Example audio output is in the [sound_samples](sound_samples/) directory.

This project started as the final project for Karl Hiner and Ben Wilfong for GA Tech CSE-6730 Modeling & Simulation, Spring 2023.
_The code as it was at the end of the school project repo is [here](https://github.com/GATech-CSE-6730-Spring-2023-Project/mesh2audio)._


Custom 2D axisymmetric FEM model designed and implemented by [Ben Wilfong](https://github.com/wilfonba).
(Ben implemented everything under the `fem` directory.)

The rest of the code is basically a GUI wrapper around Ben's axysimmetric FEM, and the O.G., [mesh2faust](https://hal.science/hal-03162901/document) by Romain Michon, Sara R Martin, and Julius O Smith, with some performance improvements like using [tetgen](https://github.com/libigl/tetgen) instead of [VegaFEM](https://viterbi-web.usc.edu/~jbarbic/vega/) for converting triangular meshes to tetrahedral, and using Eigen/[Spectra](https://github.com/yixuan/spectra) instead of MKL/[Pardiso](https://www.intel.com/content/www/us/en/docs/onemkl/developer-reference-c/2023-0/onemkl-pardiso-parallel-direct-sparse-solver-iface.html) to find the eigenvalues of the mass/stiffness matrices, for compatibility with non-Intel processors such as ARM.

## Build app

### Install dependencies

#### Mac

```shell
$ brew install glew llvm eigen
```

#### Linux

(Only tested on Ubuntu.)

```shell
$ sudo apt install llvm libc++-dev libc++abi-dev libeigen3-dev
$ ln -s llvm-config-16 llvm-config
$ export PATH="$(llvm-config --bindir):$PATH"
```

Install GTK (for native file dialogs), and OpenGL dependencies:

```shell
$ sudo apt install build-essential libgtk-3-dev libglu1-mesa-dev freeglut3-dev mesa-common-dev libglew-dev
```

### Clone, clean and build app

```shell
$ git clone --recurse-submodules git@github.com:GATech-CSE-6730-Spring-2023-Project/mesh2audio.git
$ cd mesh2audio/app
```

- **Clean:**
  - Clean up everything: `./script/Clean`
  - Clean debug build only: `./script/Clean -d [--debug]`
  - Clean release build only: `./script/Clean -r [--release]`
- **Build:**
  - Debug build (default): `./script/Build`
  - Release build: `./script/Build -r [--release]`

Debug build is generated in the `./build` directory relative to project (repo) root.
Release build is generated in `./build-release`.

On Linux, you will also need to build the `fem` executable (since the one that's committed to this repo was build on Mac):

```shell
$ sudo apt install gfortran
$ ./build.sh
```

To run the freshly built application:

```sh
# The pplication assumes it's being run from the build directory when locating its resource files.
$ cd build # or build-release
$ ./mesh2audio
```

## Stack

- App: UI/UX/mesh/audio generation & editing
  - [ImGui](https://github.com/ocornut/imgui) + [SDL3](https://github.com/libsdl-org/SDL): Immediate-mode UI/UX, supporting many environments & backends.
  - [Faust](https://github.com/grame-cncm/faust): Render the mesh to an audio graph, with real-time interactive vertex excitation.
  - [miniaudio](https://github.com/mackron/miniaudio): Continuously render the modal physical model of the input 3D volumetric mesh to audio.
  - [tetgen](https://github.com/libigl/tetgen): Convert triangular 3D meshes into tetrahedral meshes.
  - [CDT](https://github.com/artem-ogre/CDT): Constrained Delaunay Triangulation, the default 2D polygon triangulation method
  - [earcut](https://github.com/mapbox/earcut.hpp): Simpler 2D polygon triangulation, provided as an option.
  - [glm](https://github.com/g-truc/glm): Frontend maths.
  - [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended): Native file dialogs.
  - [nanosvg](https://github.com/memononen/nanosvg): Read path vertices from SVG files.
  - [ImPlot](https://github.com/epezent/implot): Plotting
  - [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo): Mesh transform and camera rotation gizmos.
  - [ImSpinner](https://github.com/dalerank/imspinner): Wicked cool loading spinners for ImGui.
- FEM:
  - 2D FEM: Custom Fortran implementation by [Ben Wilfong](https://github.com/wilfonba)
  - 3D FEM: [VegaFEM](https://github.com/grame-cncm/faust/tree/master-dev/tools/physicalModeling/mesh2faust/vega)
