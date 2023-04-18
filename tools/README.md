# vscp-espnow tools

This folder contains some useful toolsand methods to analyze esp-now frames

## Monitor mode

### Set adapter into monitor mode

You have to turn on monitor mode and choose the right channel on your wireless interface card.

Here is a example on how to do it (my USB wifi adapter is **wlx000e8e0909ec** so remember to replace references to it to your adapter) :

```
sudo ifconfig wlx000e8e0909ec down
sudo iwconfig wlx000e8e0909ec mode monitor
sudo rfkill unblock wifi; sudo rfkill unblock all
sudo ifconfig wlx000e8e0909ec up
sudo iwconfig wlx000e8e0909ec channel 3 
```

```
iwconfig 
```

can be used to list the adapters you have on your computer.


### Wifi adapter
You need to have a wifi adapter that can be set in monitor mode. Preferable also it should allo packet injection.  There are many USB wifi adapters that allow this. Also Raspberry Pi can be used if patched. See https://github.com/seemoo-lab/nexmon and https://www.youtube.com/watch?v=U3eldMLq2cc

## tcpdump

https://opensource.com/article/18/10/introduction-tcpdump
sudo tcpdump --interface wlx000e8e0909ec | grep cc:50:e3:80:23:b8

## wireshark

- Start wireshark
- Select the wireless interface. This interface must be set into monitor mode. See above.
- In wireshark set filter _wlan.sa == cc:50:e3:80:23:b8_ where  _c:50:e3:80:23:b8_ is the esp-node to investigate.
- Now you will the frames this node sends.