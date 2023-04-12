# vscp-espnow

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](http://choosealicense.com/licenses/mit/)
[![Repo Status](https://www.repostatus.org/badges/latest/wip.svg)](https://www.repostatus.org/#wip)
[![Release](https://img.shields.io/github/release/grodansparadis/vscp-espnow.svg)](https://github.com/grodansparadis/vscp-espnow/releases)


# VSCP over Espressif esp-now protocol

- [Overview](#Overview)
- [Alpha-nodes](#Alpha-nodes)
- [Beta-nodes](#Beta-nodes)
- [Gamma-nodes](#Gamma-Nodes)
- [License](#license)
- [Contribution](#contribution)

## Overview

vscp-espnow implements esp-now for ESP32 nodes using the VSCP protocol for communication. vscp-espnow is uses the [esp-idf framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

- All communication is encrypted using AES-128 CTR
- Suitable for sensor networks or control systems with lower packet count and small payload and where high response time is needed.
- vscp-espnow defines three types of nodes.
   - **Alpha nodes** have a wifi connection, a web-server, MQTT, VSCP Link server, remote logging. Other Alpha and  Beta and Gamma nodes can bind to an Alpha node forming a communication cluster that share a common encryption key. Alpha nodes are provisioned using Bluetooth.
   - **Beta nodes** are nodes that connect to a cluster using esp-now and they are *__always powered__*.
   - **Gamma nodes** are nodes that connect to a cluster using esp-now and sleep most of the time. Normally they are *__battery powered__*.
- All nodes can have firmware updated over the AIR (OTA).
- All nodes can do remote logging for easy monitoring and debugging of a device.

[VSCP (Very Simple Control Protocol)](https://www.vscp.org) is a protocol for IoT/m2m tasks that work of different transport mediums such as CAN, RS-232, Ethernet. 

[esp-now](https://www.espressif.com/en/solutions/low-power-solutions/esp-now?ct=t(EMAIL_CAMPAIGN_4_12_2023_2_38)&mc_cid=1dec54c698&mc_eid=1bb5ed46d4) is a protocol developed by [Espressif](https://www.espressif.com/en) for use with the ESP32/ESP8266 it allows for secure communication between wireless devices without a wifi connection. [espnow](https://github.com/espressif/esp-now) is a software component that makes esp-now more friendly for developers to work with.

# Alpha-nodes
Alpha nodes are nodes that is always powered. Nodes always connect to a wifi router. Alpha nodes can be connected together user either wifi or esp-now. 

Alpha nodes are provisioned (connected to wifi) using BLE.

Alpha nodes can have there firmware OTA updated. It is possible to update firmware over OTA from a remote web server or from a local file on a connected computer. Alpha nodes can also OTA update firmware on Beta and Gamma nodes. 

Have a web server. This web server allows for configuration of wifi, esp-now, VSCP, webserver, MQTT and logging etc. It can give technical information about the device. Initiating of firmware OTA upgrade is possible. Both from a secure web server and locally from a file and it can also initiate OTA for Beta/Gamma nodes if they are setup top receive upgrades.

Alpha nodes can connect with a MQTT HUB to send/receive VSCP events. 

Alpha nodes have a VSCP tcp/ip link interface that can be used for sending and receiving of VSCP events.

Remote logging is possible to setup for udp, tcp/ip, web and MQTT.

Alpha nodes implement registers and can be configured and controlled using VSCP events. A VSCP MDF file describe the node for higher level software.

Alpha nodes can act as a relay node to extend range.

All alpha nodes have an init button. 

- A short press enable the node to securely pair with other nodes, transferring encryption key and channel to work on.
- A long press (more than 3 seconds) reset the node to factory defaults.  
- A double click on the button forget wifi provision data and a need provisioning is needed.

Alpha nodes have a status led that should be green and give status information about the device.

- Blinking. Node is connecting to wifi access point or needs provisioning.
- Steady on. Node is connected.
- 

# Beta-nodes
Beta nodes are nodes that are always powered. They communicate using VSCP over esp-now and connect to alpha nodes, other beta nodes and gamma-nodes.

Beta nodes are paired with Alpha-nodes to be part of a communication cluster.

Beta nodes can send log information on different levels for debugging and diagnostics.

Alpha nodes implement registers and can be configured and controlled using VSCP events. A VSCP MDF file describe the node for higher level software.

Beta nodes can act as a relay node to extend range.

All alpha nodes have an init button. 

- A short press enable the node to securely pair with an Alpha node that is set in pairing mode, transferring encryption key and channel to work on.
- A long press (more than 3 seconds) reset the node to factory defaults.  
- A double click on the button enable the node to receive OTA firmware from an initiating (typically Alpha-node) node.

# Gamma-Nodes

Gamma nodes are nodes that are battery powered and sleep most of the time. They communicate using VSCP over esp-now and connect to alpha nodes, beta nodes and other gamma-nodes. 

Gamma nodes implement VSCP registers and can be configured and controlled using VSCP events. A VSCP MDF file describe the node for higher level software.

All gamma nodes have an init button. 

- A short press wake up a gamma node and enable the node to securely pair with an Alpha node that is set in pairing mode, transferring encryption key and channel to work on.
- A long press (more than 3 seconds) wake up a gamma node and reset the node to factory defaults.  
- A double click wake up a gamma node and enable the node to receive OTA firmware from an initiating (typically Alpha-node) node.

## Issues, Ideas And Bugs
If you have further ideas or you found some bugs, great! Create an [issue](https://github.com/grodansparadis/vscp-espnow/issues) or if you are able and willing to fix it by yourself, clone the repository and create a pull request.

## License
The whole source code is published under the [MIT license](http://choosealicense.com/licenses/mit/). Espnow is licensed as under the  [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)
Consider the different licenses of the used third party libraries too!

## Contribution
Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, shall be licensed as above, without any
additional terms or conditions.

