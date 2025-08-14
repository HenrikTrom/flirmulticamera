# 📷 flirmulticamera – v1.0.0

[![DOI](https://zenodo.org/badge/991252325.svg)](https://zenodo.org/badge/latestdoi/991252325)

C++ Library to record synchronized videos and images using [synchronizable Flir Cameras](https://flir.custhelp.com/app/answers/detail/a_id/3385/~/flir-cameras---trigger-vs.-sync-vs.-record-start) on Ubuntu. Also contains python bindings.

This module is part of my  **ROS/ROS2** [real-time 3D tracker](https://github.com/HenrikTrom/real-time-3D-tracking) and its [docker-implementation](https://github.com/HenrikTrom/ROSTrack-RT-3D).

![System Setup](content/4cams.gif)

### 📑 Citation

If you use this software, please use the GitHub **“Cite this repository”** button at the top(-right) of this page.


### 🧪 Tested with 
* Spinnaker-2.4.0.143-Ubuntu20.04-amd64-pkg.tar.gz
* 5x Flir Grasshopper GS3-U3-32S-4C
* [cpp_utils v1.0.0](https://github.com/HenrikTrom/cpp_utils)
* pybind11-dev

### Requirements

* USB Card to handle all the video input
* Synchronization cable that connects master cameras Trigger Pin to the Slave cameras input pins.
* We use `Line2` on the Master and `Line3` for the Slave cameras.
* [cpp_utils v1.0.0](https://github.com/HenrikTrom/cpp_utils)
* `fmt` and `ffmpeg`

1. Download and install the [Spinnaker SDK](https://www.teledynevisionsolutions.com/products/spinnaker-sdk/?model=Spinnaker%20SDK&vertical=machine%20vision&segment=iisflir ). Make sure that you can run Spinview and that you can get a stable video of each camera.

This repo is used for real time inference. For this purpose there is the option to use a compile time known constant for the amount of used cameras.
To enable this option use `-DUSE_ENV_DEFINED_CAMERA_COUNT=ON` in cmake and export this environment variable

```bash
FLIR_CAMERA_COUNT #Number of elements in the list above, i.e. 5
```

### Installation

You can build and install the python bindings, executables and the libraries by executing this script:

```bash
./build_install.sh
```
Otherwise, feel free to adapt use whatever installation configuration you like.

### Usage

**Python** example: 
```bash
./src/apps/get_images.py
```

CMake generates the following executables. 

`./build/record_synchronized_frame` - to save a synchronized image  

`./build/record_synchronized_videos` - to record synchronized videos 

You can specify the camera settings by passing a `<your-settings>.json` to each executable as first argument:
```bash
 ./build/record_synchronized_frame /home/docker/modules/FlirMultiCamera/cfg/1024x768.json
 ```
See the json scheme in `./cfg`. 

For the video executable you can further specify the number of frames by passing them as second argument:

```bash
./build/record_synchronized_videos /home/docker/workspace/build/dependencies/FlirMultiCamera/cfg/1024x768.json 40
```

Otherwise the executables will assume the default configuration 1024x768.json and a default frame number of 30. 

You can specify the **savepath** of your images/videos in the the config. If `save_dir` is an empty string it will chose the ./outputs folder as default.  

#### Notes

This software is an essential part of my [docker-based pipeline for online 3D tracking](https://github.com/HenrikTrom/Docker-ROS-Online3D-Tracking), that I build during my PhD.