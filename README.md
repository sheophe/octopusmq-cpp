octopusMQ
=========

multi-protocol message broker for mesh networks and decentralized data processing nodes.

based on publish/subscribe model, it currently supports only two protocols: MQTTand DSS. 

each octopusMQ instance has a single message queue and could have multiple connections to other instances via *adapters*. adapters are network nodes, which could be connected to different interfaces, working with different protocols with configurable roles (*broker* or *client* in case of MQTT).
