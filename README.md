# vscp-espnow

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](http://choosealicense.com/licenses/mit/)
[![Repo Status](https://www.repostatus.org/badges/latest/wip.svg)](https://www.repostatus.org/#wip)
[![Release](https://img.shields.io/github/release/grodansparadis/vscp-espnow.svg)](https://github.com/grodansparadis/vscp-espnow/releases)

![](./images/vscp_logo_text_box_200.png)

# VSCP over [Espressif](https://www.espressif.com/) esp-now protocol

- [Overview](#Overview)
- [Alpha-nodes](#Alpha-nodes)
- [Beta-nodes](#Beta-nodes)
- [Gamma-nodes](#Gamma-Nodes)
- [Testing](#Testing)
- [MQTT](#MQTT)
- [VSCP-Works](#Using VSCP Works with vscp-espnow)
- [VSCP-Daemon](#Using a VSCP daemon with vscp-espnow)
- [License](#license)
- [Contribution](#contribution)

_An initial note on [espnow](https://github.com/espressif/esp-now) vs. [esp-now](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html). Below when we say **esp-now** it is esp-now as of the protocol developed by Espressif and is part of esp-idf. When we say **espnow** we talk about the component for espnow that is developed to sit over esp-now for more usability. Espnow also is developed by Espressif._

## Overview

vscp-espnow implements esp-now/espnow for ESP32 nodes using the VSCP protocol for application level communication. vscp-espnow is using the [esp-idf framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

- All communication is encrypted.
- VSCP level II events is used to control and share information. Giving a common way for equipment with different origin to work together. 
- Suitable for sensor networks or control systems. High response time. 
- Nodes can forward events and extend over larger areas.
- vscp-espnow defines three types of nodes.
   - **Alpha nodes** have a wifi connection, a web-server, MQTT, optional VSCP Link server, remote logging. Other Alpha and  Beta and Gamma nodes can bind to an Alpha node forming a communication cluster that share a common encryption key and communication channel. Alpha nodes are provisioned using Bluetooth.
   - **Beta nodes** are nodes that connect to a cluster using esp-now and they are *__always powered__*. A beta node is added to the cluster by interaction with an Alpha node.
   - **Gamma nodes** are nodes that connect to a cluster using esp-now and sleep most of the time. A gamma node is added to the cluster by interaction with an Alpha node. Normally they are *__battery powered__*.
- All nodes can have firmware updated over the AIR (OTA).
- All nodes can do remote logging for easy monitoring and debugging of a device.

The vscp-espnow firmware is **NOT** a ready made solutions that you can upload into a device and start to use. The code here is the initial development code for modules based on vscp-espnow firmware containg low level functionality. [Grodans Paradis AB](https://www.grodansparadis.com) will develop such modules and hopefully others will do so to. A list will be provided later.

[VSCP (Very Simple Control Protocol)](https://www.vscp.org) is a protocol for IoT/m2m tasks that work of different transport mediums such as CAN, RS-232, Ethernet. 

[esp-now](https://www.espressif.com/en/solutions/low-power-solutions/esp-now?ct=t(EMAIL_CAMPAIGN_4_12_2023_2_38)&mc_cid=1dec54c698&mc_eid=1bb5ed46d4) is a protocol developed by [Espressif](https://www.espressif.com/en) for use with the ESP32/ESP8266 it allows for secure communication between wireless devices without a wifi connection. [espnow](https://github.com/espressif/esp-now) is a software component that makes esp-now more friendly for developers to work with.

# Alpha-nodes
Alpha nodes are devices that are always powered. They are also always connect to a wifi router. Alpha nodes can be connected together user either wifi or esp-now. 

Alpha nodes are provisioned (connected to wifi) using BLE.

Alpha nodes can have there firmware OTA updated. It is possible to update firmware over OTA from a remote web server or from a local file on a connected computer. Alpha nodes can also OTA update firmware on remote Beta and Gamma nodes. 

Alpha nodes have a local web server. This web server allows for configuration of wifi, esp-now, VSCP, webserver, MQTT and logging etc. It can give technical and status information about the device. Initiating of firmware OTA upgrade is possible. Both from a secure web server and locally from a file and it can also initiate OTA for Beta/Gamma nodes if they are setup to receive upgrades.

Alpha nodes can connect with a MQTT HUB to send/receive VSCP events. 

Alpha nodes optionally have a VSCP tcp/ip link interface that can be used for sending and receiving of VSCP events as an alternative to the MQTT link.

Remote logging is possible to setup for udp, tcp/ip, web and MQTT.

Alpha nodes act as time severs for the segment they are on. Keeping clocks updated an all nodes in a segment.

Alpha nodes implement [VSCP registers](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_register_abstraction_model) and have an optional [decision matrix](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_decision_matrix) and can be configured and controlled using VSCP events. A VSCP [MDF file](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_module_description_file) describe the node for higher level software.

Alpha nodes can act as a relay node to extend range.

All alpha nodes have an init button. 

- A short press enable the node to securely pair with other nodes, transferring encryption key and a common communication channel to work on.
- A long press (more than 3 seconds) reset the node to factory defaults. This forget wifi provision data and a new provisioning is needed.  
- A double click on the button starts wifi provisioning using BLE.

Alpha nodes have a status led that should be green and give status information about the device.

- Blinking. Node is connecting to wifi access point or needs provisioning.
- Steady on. Node is connected.
- tbd

# Beta-nodes
Beta nodes are nodes that are always powered. They communicate using VSCP over esp-now and connect to alpha nodes, other beta nodes and gamma-nodes.

Beta nodes are paired with Alpha-nodes to be part of a communication cluster.

Beta nodes can send log information on different levels for remote debugging and diagnostics.

Beta nodes implement [VSCP registers](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_register_abstraction_model) and have an optional [decision matrix](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_decision_matrix) and can be configured and controlled using VSCP events. A VSCP [MDF file](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_module_description_file) describe the node for higher level software.

Beta nodes can act as a relay node to extend range.

All alpha nodes have an init button. 

- A short press enable the node to securely pair with an Alpha node that is set in pairing mode, transferring encryption key and a common communication channel to work on.
- A long press (more than 3 seconds) reset the node to factory defaults.  
- A double click on the button enable the node to receive OTA firmware from an initiating (typically Alpha-node) node.

# Gamma-Nodes

Gamma nodes are nodes that are battery powered and sleep most of the time. They communicate using VSCP over esp-now and connect to alpha nodes, beta nodes and other gamma-nodes. They can be very low power by just wake up for short time ehen needed and  when that hgappens tell the system they are awake, send events (measuremenst, alarms, key-clicks etc) they have to send and receive and act on data that is waiting to it. A MQTT broker can be used as a postbox to hold events that the gamma node is the received of. An Alpha node takes care of this transfer when the Gamma node awakes.

Gamma nodes implement [VSCP registers](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_register_abstraction_model) and have an optional [decision matrix](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_decision_matrix) and can be configured and controlled using VSCP events. A VSCP [MDF file](https://grodansparadis.github.io/vscp-doc-spec/#/./vscp_module_description_file) describe the node for higher level software.

All gamma nodes have an init button. 

- A short press wake up a gamma node and enable the node to securely pair with an Alpha node that is set in pairing mode, transferring encryption key and channel to work on.
- A long press (more than 3 seconds) wake up a gamma node and reset the node to factory defaults.  
- A double click wake up a gamma node and enable the node to receive OTA firmware from an initiating (typically Alpha-node) node.

Firmware nodes wake up on even/uneven intervals and is available for control and to give status.

## Test
To test vscp-espnow you need at least two ESP32 boards. One should be loaded with the alpha firmware and one with the beta or gamma firmware depending on your needs. There is no (well there is in practice) limit on the number of Beta and Gamma nodes you can set up. It is also possible to have several Alpha nodes.

### Set up Alpha node
First you need to set up the Alpha node to connect to the wifi router. Start the node and it is ready for provisioning. This is done with your phone over BLE. There is an app for Android [here](https://play.google.com/store/apps/details?id=com.espressif.provble) and for iOS [here](https://apps.apple.com/in/app/esp-ble-provisioning/id1473590141). 

A ready made product will have a QR code labeled on it but for development you probably need to use the UART of the device to get this information. The status led on the device blinks as long as the device is not connected to wifi. When the len is steady on you have a connection and is ready to move on. 

Now look up the node by scanning for its ip-address. Open this ip address in a web browser and you will go to the configuration interface. Default user is **vscp** and default password **secret**. For a non product test you should probably change this the first thing you do. This is the first page what you will meet

![](./images/config-main.png)

Here you can configure the module, [scan wifi network](./images/config-wifi.png), setup espnow parameters, [set up VSCP link interface](./images/config-vscplink.png), Set up [web server](./images/config-websrv.png), set up [MQTT](./images/config-mqtt.png), setup [logging](./images/config-logging.png). You can also get device information, do [OTA updates](./images/ota.png) of local or remote firmware, and [provisioning nodes](./images/prov.png) on the segment. And more...

The Alpha node generate a 32 byte secret key itself when it starts for the first time. But you can set this key also in the configuration interface as a 32 byte hex-string. It is important to note that restoring factory defaults on an alpha node means you have to attach all other nodes in the segment again to this node.

If you want to use MQTT now configure MQTT to use a MQTT broker you have access to in the configuration interface.

Hold down the init key for more then four seconds to restore factory defaults. It starts to blink again when it is ready. You need to proivision the node again so it get access to wifi. All connected Beta and Gamma nodes needs to be set up agin also if you do this. You can set your own key in the web interface under configure/module/primary key. The key should be a 32 byte key on hex form (_000102030405060708090A0B...._a total of 64 hex characters). The key should not change after you have added Beta and Gamma nodes to the segment the Alpha node is on.

You can use VSCP to configure the alpha node (and nodes connected to it) once the MQTT or the VSCP link interface is configured. You can do control, configuration and remote debugging this way on  a live system.

### Set up Beta node(s)

Power up the beta node. The led blinks to indicate it is not part of a segment yet. Press the init key shortly on both the alpha node and the beta node and wait until the status led on the beta node lights steady. Now it is part of the same segment as the Alpha node. You can activate this process using VSCP on the alpha node also if you want.

Hold down the init key for more then four seconds to restore factory defaults. It starts to blink again when it have restarted and is ready to be paired with a node in the cluster.

Double click the init button on the alpha node to start an OTA update. An alpha node will deliver a new OTA firmware image other from a local file or from a server. 

### Set up Gamma nodes node(s)

tbd but the process is mostly like an beta-node

## MQTT
As always for VSCP the segment you build is not dependent on a server. You can set ut up and the it will do it's work. Bit often one want to interact with a world outside of a vscp-now segment and then MQTT is the perfect candidate.

The default MQTT broker is 192.168.1.1 and that is probably not the one you use. Change the the address and other parameters. If You use the default publishing topic you can subscribe to topic published from an alpha node that publish all traffic from the espnow segment.

```
mosquitto_sub -h 192.168.1.7 -p 1883 -u vscp -P secret -t vscp/FF:FF:FF:FF:FF:FF:FF:FE:B8:27:EB:CF:3A:15:FF:FF/#
```

The **FF:FF:FF:FF:FF:FF:FF:FE:B8:27:EB:CF:3A:15:FF:FF** is the GUID for the alpha node that publish the events. It will be different in your case. Check the GUID in the **Node Information** of the web interface.

The default publishing topic for an alpha node is

```
vscp/{{guid}}/{{class}}/{{type}}/{{index}}
```

This is pretty standard for most VSCP devices to use over MQTT. It allows for filtering of GUID, VSCP class and VSCP type as well as senor index for measurments. But you can of course use any other schema.  The mosquitto_sub sample above subscribe to all events. But if you are only interested in temperature events for example use

```
vscp/FF:FF:FF:FF:FF:FF:FF:FE:B8:27:EB:CF:3A:15:FF:FF/10/6/#
```

To use **{{eguid}}** instead of **{{guid}}** let you subscribe onlu to events sent by a specific node.

Default subscribe on Alpha nodes use

```
vscp/{{guid}}/pub/#
```

And you can publish events to the bus by publish them on this topic on JSON format. {{guid}} will again be changed to the full GUID of the Alpha node.

Other options you have are

| Mustach token | Description |
| ============= | =========== |
| {{node}}        | Node name in clear text of Alpha node |
| {{guid}}        | Node GUID for Alpha node |
| {{evguid}}      | Event GUID for the event published |
| {{class}}       | VSCP Event class |
| {{type}}        | VSCP Event type |
| {{nickname}}    | Node nickname (16-bit) for Alpha node |
| {{evnickname}}  | Node nickname (16-bit) for node thats ent event. |
| {{sindex}}      | Sensor index (if any) |

## Using VSCP Works with vscp-espnow

Will be added.  

VSCP Works is available [here](https://github.com/grodansparadis/vscp-works-qt) and old version is [here](https://github.com/grodansparadis/vscpworks).

## Using a VSCP daemon with vscp-espnow
Will be added.

The VSCP daemon is available [here](https://github.com/grodansparadis/vscp)

## Issues, Ideas And Bugs
If you have further ideas or you found some bugs, great! Create an [issue](https://github.com/grodansparadis/vscp-espnow/issues) or if you are able and willing to fix it by yourself, clone the repository and create a pull request.

## License
The whole source code is published under the [MIT license](http://choosealicense.com/licenses/mit/). Espnow is licensed under the  [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)
Consider the different licenses of possible third party libraries too!

## Contribution
Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, shall be licensed as above, without any
additional terms or conditions.

