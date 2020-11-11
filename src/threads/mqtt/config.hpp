#ifndef OCTOMQ_MQTT_CONFIG_H_
#define OCTOMQ_MQTT_CONFIG_H_

#define MQTT_USE_WS
#ifdef OCTOMQ_ENABLE_TLS
#define MQTT_USE_TLS
#endif
#define MQTT_STD_OPTIONAL
#define MQTT_STD_VARIANT
#define MQTT_STD_STRING_VIEW
#define MQTT_STD_ANY
#define MQTT_NS mqtt_cpp

#endif
