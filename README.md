# MQTT Bridge - Tool to transfer data from an MQTT broker to another MQTT broker
[![GitHub sourcecode](https://img.shields.io/badge/Source-GitHub-green)](https://github.com/pvtom/mqttbridge/)
[![GitHub last commit](https://img.shields.io/github/last-commit/pvtom/mqttbridge)](https://github.com/pvtom/mqttbridge/commits)
[![GitHub issues](https://img.shields.io/github/issues/pvtom/mqttbridge)](https://github.com/pvtom/mqttbridge/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/pvtom/mqttbridge)](https://github.com/pvtom/mqttbridge/pulls)
[![GitHub](https://img.shields.io/github/license/pvtom/mqttbridge)](https://github.com/pvtom/mqttbridge/blob/main/LICENSE)

This software module transfers data for selected topics from an MQTT broker to another MQTT broker.

If you don't like to configure your MQTT broker for bridging you can choose this tool.

The solution is based on the mqttsub tool (https://gist.github.com/pvtom --> mqttsub.c)

## Prerequisite

mqttbridge needs the library libmosquitto. For installation please enter:

```
sudo apt-get install libmosquitto-dev
```

## Cloning the Repository

```
git clone https://github.com/pvtom/mqttbridge.git
cd mqttbridge
```

## Compilation

To build the program use
```
gcc mqttbridge.c -o mqttbridge -lmosquitto
```

## Usage

Please start the program with
```
./mqttbridge
```

The available parameters will be displayed.

## Example
```
./mqttbridge --src_host host1 --dst_host host2 -t "smarthome/#" -t "devices/+/temperature"
```
