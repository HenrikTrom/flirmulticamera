rm -rf build; mkdir build

cd build

cmake \
    -DBUILD_EXECUTABLES=ON \
    -DGENERATE_PYTHON_BINDINGS=ON \
    -DUSE_ENV_DEFINED_CAMERA_COUNT=ON \
    ..
    

make -j$(nproc)

sudo make install