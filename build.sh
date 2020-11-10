#!/bin/sh
if [ ! -d "./build" ]; then
    mkdir build/
fi

OCTOMQ_OPT_FLAGS=""

for var in "$@"
do
    case "$var" in
        --optimize)
            OCTOMQ_OPT_FLAGS="$OCTOMQ_OPT_FLAGS -D OCTOMQ_RELEASE_COMPILATION=ON"
            ;;
        --tls)
            OCTOMQ_OPT_FLAGS="$OCTOMQ_OPT_FLAGS -D OCTOMQ_ENABLE_TLS=ON"
            ;;
        --clean)
            rm -rf build/
            mkdir build/
            ;;
    esac
done

cd ./build
cmake $OCTOMQ_OPT_FLAGS ../
cmake --build . --target octopusmq -- -j 8

unset OCTOMQ_OPT_FLAGS
