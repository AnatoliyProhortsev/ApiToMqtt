# Api To Mqtt Test-client

## Dependencies
- libpaho-mqttpp-dev
- libcurl4-openssl-dev
- libxml2-dev
- mosquitto
## Build
```bash
mkdir build && cd build && cmake .. && cmake --build .
```
## Run
run mosquitto
```bash
cd configs && mosquitto -c mosquitto.conf -v
```
run client
```bash
cd bin && ./ApiToMqtt
```