#!/bin/sh
if [ ! -d "./build" ]; then
    mkdir ./build
fi
cd ./build
cmake ../
cmake --build . --target octopusmq -- -j 8
