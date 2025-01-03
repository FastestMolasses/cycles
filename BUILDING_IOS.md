make update

mkdir build-ios
cd build-ios
cmake .. -DIOS=ON -DCMAKE_TOOLCHAIN_FILE=../ios.toolchain.cmake -GXcode
cmake --build . --config Release
