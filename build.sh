#!/bin/sh
OCTOMQ_OPT_FLAGS=unset

usage()
{
    echo "usage: build.sh [ -c | --clean ] [ --optimize ] [ --tls ]"
    exit 2
}

for var in "$@"
do
    case "$var" in
        -c | --clean)
            echo "-- Cleaning build directory"
            rm -rf build/
            mkdir build/
            ;;
        --optimize)
            OCTOMQ_OPT_FLAGS="$OCTOMQ_OPT_FLAGS -D OCTOMQ_RELEASE_COMPILATION=ON"
            ;;
        --tls)
            OCTOMQ_OPT_FLAGS="$OCTOMQ_OPT_FLAGS -D OCTOMQ_ENABLE_TLS=ON"
            ;;
        --help)
            usage
            ;;
        *)
            echo "error: unknown option $var"
            usage
            ;;
    esac
done

if [ ! -d "./build" ]; then
    rm -f ./build
    mkdir ./build/
fi

cd ./build
cmake $OCTOMQ_OPT_FLAGS ../
cmake --build . --target octopusmq -- -j 8

unset OCTOMQ_OPT_FLAGS OCTOMQ_MAKE_JOBS
