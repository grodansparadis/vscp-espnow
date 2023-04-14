# vscp-espnow-receive

This file builds on work done by Florenc Caminadem Thomas FLayols, Etienne Arlaud see LICENSE file for full information. Original code can be found [here](https://github.com/thomasfla/Linux-ESPNOW/blob/master/ESPNOW_lib/src/main.cpp)

This program dump received vscp-espnow frames on a wifi channel of choice. Note that the frames here have the extensions to esp-now added [here](https://github.com/espressif/esp-now) and that the payload is A VSCP event on the format described in this repo.

First find the wifi interface you want to use. The interface must be able to be set in monitor mode.

```
iwconfig
```

or

```
sudo ip link list
```


to get wifi adapter. The adapter needs to support monitor mode. In my case the adapter is

```
wlx000e8e0909ec
```

https://github.com/thomasfla/Linux-ESPNOW/tree/master/ESPNOW_lib

Do not forget to turn on monitor mode and choose the right channel on your wireless interface card.

Here is a example on how to do it :

```
sudo ifconfig wlx000e8e0909ec down
sudo iwconfig wlx000e8e0909ec mode monitor
sudo rfkill unblock wifi; sudo rfkill unblock all
sudo ifconfig wlx000e8e0909ec up
sudo iwconfig wlx000e8e0909ec channel 3 
```

If you get 

   "SIOCSIFFLAGS: Operation not possible due to RF-kill"

when you set the interface in up state follow the steps found here

https://askubuntu.com/questions/62166/siocsifflags-operation-not-possible-due-to-rf-kill

For me

```
sudo rfkill unblock wifi; sudo rfkill unblock all
```

is enough.

To start to log frames execute

```
sudo ./bin/vscp-espnow-receiver wlx000e8e0909ec
```

## Reference

- Origional code: https://github.com/thomasfla/Linux-ESPNOW/blob/master/ESPNOW_lib/src/main.cpp
- esp-now frame format: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
- Soft blocking: https://askubuntu.com/questions/62166/siocsifflags-operation-not-possible-due-to-rf-kill
- How to use packet injection with mac80211: https://www.kernel.org/doc/Documentation/networking/mac80211-injection.txt
- RadioTap: https://www.radiotap.org/  and https://github.com/radiotap/radiotap.github.io/blob/master/index.md