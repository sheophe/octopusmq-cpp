#!/bin/sh
usage()
{
    echo "usage: run.sh [ -b | --bridged ]"
    exit 2
}

OCTOMQ_CONFIG_FILE=./octopus.json

for var in "$@"
do
    case "$var" in
        -b | --bridged)
            OCTOMQ_CONFIG_FILE=./octopus_bridged.json
            ;;
        *)
            echo "error: unknown option $var"
            usage
            ;;
    esac
done

./build/octopusmq $OCTOMQ_CONFIG_FILE
