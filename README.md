# vscp-espnow

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](http://choosealicense.com/licenses/mit/)
[![Repo Status](https://www.repostatus.org/badges/latest/wip.svg)](https://www.repostatus.org/#wip)
[![Release](https://img.shields.io/github/release/grodansparadis/vscp-espnow.svg)](https://github.com/grodansparadis/vscp-espnow/releases)


# VSCP over Espressif esp-now protocol

- [Overview](#Overview)
- [License](#license)
- [Contribution](#contribution)

## Overview

vscp-espnow implements esp-now for ESP32 nodes using the VSCP protocol for communication.

- All communication is encrypted using AES-128 CTR
- vscp-espnow defines three types of nodes.
   - **Alpha nodes** have a wifi connection, a web-server, MQTT, VSCP Link server, remote logging. Beta and Gamma nodes can bind to an Alpha node forming a communication cluster. Alpha nodes are provisioned using Bluetooth.
   - **Beta nodes** are nodes that connect to a cluster using esp-now and they are always powered.
   - **Gamma nodes** are nodes that connect to a cluster using esp-now and sleep most of the time. Normally they are battery powered.
- All nodes can have firmware updated over the AIR (OTA).
- All nodes can do remote logging for easy monitoring and debugging of a remote device.

[VSCP (Very Simple Control Protocol)](https://www.vscp.org) is a protocol for IoT/m2m tasks that work of different transport mediums such as CAN, RS-232, Ethernet. 

[esp-now](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) is a protocol developed by [Espressif](https://www.espressif.com/en) for use with the ESP32/ESP8266 it allows for secure communication between wireless devices without a wifi connection. [espnow](https://github.com/espressif/esp-now) is a software component that makes esp-now more friendly for developers to work with.



## Issues, Ideas And Bugs
If you have further ideas or you found some bugs, great! Create an [issue](https://github.com/grodansparadis/vscp-espnow/issues) or if you are able and willing to fix it by yourself, clone the repository and create a pull request.

## License
The whole source code is published under the [MIT license](http://choosealicense.com/licenses/mit/).
Consider the different licenses of the used third party libraries too!

## Contribution
Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, shall be licensed as above, without any
additional terms or conditions.

