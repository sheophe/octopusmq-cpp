octopusMQ
=========

Multi-protocol message broker for mesh networks and decentralized data processing nodes.

Based on publish/subscribe model, it currently supports only two protocols: MQTT and DDS. 

Each octopusMQ instance has a single message queue and could have multiple connections to other instances via *adapters*. Adapters are network nodes, which could be connected to different network interfaces, working with different protocols with configurable roles (*broker* or *client* in case of MQTT).

Build
-----

CMake and Boost library are required to build this project.

On Debian GNU/Linux:
```
sudo apt install cmake libboost-dev
```

Then build the project using build script:
```
git submodule init
git submodule update
./build.sh --clean --static --optimize --no-dds
```

To run using octopusmq.json as configuration file:
```
./build/octopusmq ./octopusmq.json
```
