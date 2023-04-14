/*
Florenc Caminade
Thomas FLayols
Etienne Arlaud

Receive raw 802.11 packet and filter ESP-NOW vendor specific action frame using
BPF filters. https://hackaday.io/project/161896
https://github.com/thomasfla/Linux-ESPNOW

Adapted from :
https://stackoverflow.com/questions/10824827/raw-sockets-communication-over-wifi-receiver-not-able-to-receive-packets

1/Find your wifi interface:
$ iwconfig

2/Setup your interface in monitor mode :
$ sudo ifconfig wlp5s0 down
$ sudo iwconfig wlp5s0 mode monitor
$ sudo ifconfig wlp5s0 up

3/Run this code as root

-------------------------------------------------------------------------------

Changes and additions 2023 by Ake Hedman for the VSCP project (https://vscp.org)

*/
#include <arpa/inet.h>
#include <assert.h>
#include <linux/filter.h>
#include <linux/if_arp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define PACKET_LENGTH  400 // Approximate
#define MYDATA         18  // 0x12
#define MAX_PACKET_LEN 1000

/*our MAC address*/
//{0xF8, 0x1A, 0x67, 0xB7, 0xeB, 0x0B};

/*ESP8266 host MAC address*/
//{0x84,0xF3,0xEB,0x73,0x55,0x0D};

///////////////////////////////////////////////////////////////////////////////
// filter
//
// filter action frame packets
//   Equivalent for tcp dump :
//     type 0 subtype 0xd0 and wlan[24:4]=0x7f18fe34 and wlan[32]=221 and
//     wlan[33:4]&0xffffff = 0x18fe34 and wlan[37]=0x4
// NB : There is no filter on source or destination addresses, so this code will
// 'receive' the action frames sent by this computer...

#define FILTER_LENGTH 20

/* clang-format off */
static struct sock_filter bpfcode[FILTER_LENGTH] = {
  { 0x30, 0, 0, 0x00000003 },	// ldb [3]	                    // radiotap header length : MS byte
  { 0x64, 0, 0, 0x00000008 },	// lsh #8	                      // left shift it
  { 0x7, 0, 0, 0x00000000 },	// tax		                      // 'store' it in X register
  { 0x30, 0, 0, 0x00000002 },	// ldb [2]	                    // radiotap header length : LS byte
  { 0x4c, 0, 0, 0x00000000 },	// or  x	                      // combine A & X to get radiotap header length in A
  { 0x7, 0, 0, 0x00000000 },	// tax		                      // 'store' it in X
  { 0x50, 0, 0, 0x00000000 },	// ldb [x + 0]		              // right after radiotap header is the type and subtype
  { 0x54, 0, 0, 0x000000fc },	// and #0xfc		                // mask the interesting bits, a.k.a 0b1111 1100
  { 0x15, 0, 10, 0x000000d0 },	// jeq #0xd0 jt 9 jf 19	      // compare the types (0) and subtypes (0xd)
  { 0x40, 0, 0, 0x00000018 },	// Ld  [x + 24]			            // 24 bytes after radiotap header is the end of MAC header, so it is category and OUI (for action frame layer)
  { 0x15, 0, 8, 0x7f18fe34 },	// jeq #0x7f18fe34 jt 11 jf 19	// Compare with category = 127 (Vendor specific) and OUI 18:fe:34
  { 0x50, 0, 0, 0x00000020 },	// ldb [x + 32]				          // Beginning of Vendor specific content + 4 ?random? bytes : element id
  { 0x15, 0, 6, 0x000000dd },	// jeq #0xdd jt 13 jf 19		    // element id should be 221 (according to the doc)
  { 0x40, 0, 0, 0x00000021 },	// Ld  [x + 33]				          // OUI (again!) on 3 LS bytes
  { 0x54, 0, 0, 0x00ffffff },	// and #0xffffff			          // Mask the 3 LS bytes
  { 0x15, 0, 3, 0x0018fe34 },	// jeq #0x18fe34 jt 16 jf 19		// Compare with OUI 18:fe:34
  { 0x50, 0, 0, 0x00000025 },	// ldb [x + 37]				          // Type
  { 0x15, 0, 1, 0x00000004 },	// jeq #0x4 jt 18 jf 19			    // Compare type with type 0x4 (corresponding to ESP_NOW)
  { 0x6, 0, 0, 0x00040000 },	// ret #262144	                // return 'True'
  { 0x6, 0, 0, 0x00000000 },	// ret #0	                      // return 'False'
};
/* clang-format on */

///////////////////////////////////////////////////////////////////////////////
// print_packet
//

void
print_packet(uint8_t *data, int len)
{
  int bEncrypted = 0;

  printf("------ esp-now frame ------\n");

  // Radiotap bps
  if (len >= 10) {
    switch (data[9]) {
      case 0x02:
        printf("Speed=\033[0;35m1Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x04:
        printf("Speed=\033[0;35m2Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x0C:
        printf("Speed=\033[0;35m6Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x12:
        printf("Speed=\033[0;35m8Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x18:
        printf("Speed=\033[0;35m12Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x24:
        printf("Speed=\033[0;35m18Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x30:
        printf("Speed=\033[0;35m24Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x48:
        printf("Speed=\033[0;35m36Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x60:
        printf("Speed=\033[0;35m48Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      case 0x6C:
        printf("Speed=\033[0;35m54Mbps [0x%02X]\033[0;0m \t ", data[9]);
        break;
      default:
        printf("Data rate is unknown \033[0;35m[0x%02X]\033[0;0m \t ", data[9]);
        break;
    }
  }

  // RadioTap Channel
  if (len >= 12) {

    switch ((data[11] << 8) + data[10]) {

      case 2412:
        printf("Channel=\033[0;35m1 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2417:
        printf("Channel=\033[0;35m2 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2422:
        printf("Channel=\033[0;35m3 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2427:
        printf("Channel=\033[0;35m4 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2432:
        printf("Channel=\033[0;35m5 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2437:
        printf("Channel=\033[0;35m6 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2442:
        printf("Channel=\033[0;35m7 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2447:
        printf("Channel=\033[0;35m8 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2452:
        printf("Channel=\033[0;35m9 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2457:
        printf("Channel=\033[0;35m10 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2462:
        printf("Channel=\033[0;35m11 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2467:
        printf("Channel=\033[0;35m12 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      case 2472:
        printf("Channel=\033[0;35m13 (%d)\033[0;0m \t ", (data[11] << 8) + data[10]);
        break;

      default:
        break;
    }
  }

  // MAC Header
  if (len >= 41) {
    printf("\nDest=\033[0;35m%02X:%02X:%02X:%02X:%02X:%02X\033[0;0m \t ", data[22], data[23], data[24], data[25], data[26], data[27]);
    printf("Source=\033[0;35m%02X:%02X:%02X:%02X:%02X:%02X\033[0;0m \t ", data[28], data[29], data[30], data[31], data[32], data[33]);
    printf("Broadcast=\033[0;35m%02X:%02X:%02X:%02X:%02X:%02X\033[0;0m  \n", data[34], data[35], data[36], data[37], data[38], data[39]);
  }

  // esp-now
  if (len >= 56) {
    printf("Category Code=\033[0;35m0x%02X\033[0;0m  %s\t ", data[42], (data[42] == 0x7f) ? "\033[0;32mOK\033[0;0m " : "\033[0;31m!OK\033[0;0m ");
    printf("Org id=\033[0;35m0x%02X 0x%02X 0x%02X\033[0;0m  %s \t ",
           data[43],
           data[44],
           data[45],
           (data[43] == 0x18 && data[44] == 0xFE && data[45] == 0x34) ? "\033[0;32mOK\033[0;0m " : "\033[0;31m!OK\033[0;0m ");
    printf("Random=\033[0;35m0x%02X%02X%02X%02X\033[0;0m \t ", data[46], data[47], data[48], data[49]);
    printf("Element id=\033[0;35m0x%02X\033[0;0m  %s \n", data[50], (data[50] == 0xdd) ? "\033[0;32mOK\033[0;0m " : "\033[0;31m!OK\033[0;0m ");
    printf("Length=\033[0;35m%d\033[0;0m \t\t ", data[51]);
    printf("Org id=\033[0;35m0x%02X 0x%02X 0x%02X\033[0;0m  %s\t ",
           data[52],
           data[53],
           data[54],
           (data[52] == 0x18 && data[53] == 0xFE && data[54] == 0x34) ? "\033[0;32mOK\033[0;0m " : "\033[0;31m!OK\033[0;0m ");
    printf("esp-now=\033[0;35m0x%02X\033[0;0m  %s\t ", data[55], (data[55] == 0x04) ? "\033[0;32mOK\033[0;0m " : "\033[31;0m!OK\033[0;0m ");
    printf("Version=\033[0;35m0x%02X\033[0;0m \n", data[56]);
  }

  printf("-------------------------------------------------------------------------------------------\n");

  // ESP-NOW
  if (len >= 76) {
    printf("Magic=\033[0;35m0x%02X%02X\033[0m\t\t ", data[57], data[58]);
    //printf("Type=\033[0;35m%d\033[0m\t\t\t ", data[59] & 0x0f);

    // NOTE!!!! Out of order
    printf("Ver=\033[0;35m%d\033[0m\t\t\t\t", (data[59] & 0x30) >> 4);
    printf("Size=\033[0;35m%d\033[0m\t\t", data[60]);

    // Type
    switch (data[59] & 0x0f) {

      case 0:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_ACK (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 1:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_FORWARD (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 2:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_GROUP (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 3:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_PROV (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 4:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_CONTROL_BIND (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 5:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_CONTROL_DATA (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 6:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_OTA_STATUS (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 7:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_OTA_DATA (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 8:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_DEBUG_LOG (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 9:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_DEBUG_COMMAND (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 10:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_DATA (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 11:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_SECURITY_STATUS (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 12:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_SECURITY (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 13:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_SECURITY_DATA (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      case 14:
        printf("Type=\033[0;35mESPNOW_DATA_TYPE_RESERVED (%d)\033[0m\t ", data[59] & 0x0f);
        break;

      default:
        printf("Type=\033[0;35mUnknown (%d)\033[0m\t ", data[59] & 0x0f);
        break;
    }

    
    uint32_t val = (data[64] << 24) + (data[63] << 16) + (data[62] << 8) + data[61];
    bEncrypted = val & 0x40;
    printf("\n\033[0;45mFlags:\033[0m channel=\033[0;35m%d\033[0m\t ", val & 0x0f);
    printf("Filter adj. channel=\033[0;35m%d\033[0m\t\t ", (val & 0x10) ? 1 : 0);
    printf("Filter weak signal=\033[0;35m%d\033[0m\t ", (val & 0x20) ? 1 : 0);
    printf("Security=\033[0;35m%d\033[0m\t ", (val & 0x40) ? 1 : 0);    
    printf("Broadcast=\033[0;35m%d\033[0m\t ", (val & 0x800) ? 1 : 0);
    printf("Group=\033[0;35m%d\033[0m \n", (val & 0x1000) ? 1 : 0);
    printf("ACK=\033[0;35m%d\033[0m\t\t\t ", (val & 0x2000) ? 1 : 0);
    printf("Retransmit cnt=\033[0;35m%d\033[0m\t\t ", (val & 0x0007c000) >> 14);
    printf("Forward TTL=\033[0;35m%d\033[0m\t\t ", (val & 0x00F80000) >> 19);
    printf("Forward RSSI=\033[0;35m%d\033[0m \n", (int8_t) data[64]);
    printf("Dest=\033[0;35m%02X:%02X:%02X:%02X:%02X:%02X\033[0m\t ", data[65], data[66], data[67], data[68], data[69], data[70]);
    printf("Orig. Addr=\033[0;35m%02X:%02X:%02X:%02X:%02X:%02X\033[0m \n", data[71], data[72], data[73], data[74], data[75], data[76]);
  }

  printf("-------------------------------------------------------------------------------------------\n");

  if (len > 92 && data[77] == 0x55 && data[78] == 0xaa) {

    printf("\032[31mPayload is VSCP\033[0m: \033[0;35m0x%02X 0x%02X\033[0;0m ", data[77], data[78]);

    switch (data[79] & 0x0f) {
      case 0:
        printf("Encryption=\033[0;35mnone\033[0;0m ");
        break;
      case 1:
        printf("Encryption=\033[0;35mAES128\033[0;0m ");
        break;
      case 2:
        printf("Encryption=\033[0;35mAES192\033[0;0m ");
        break;
      case 3:
        printf("Encryption=\033[0;35mAES256\033[0;0m ");
        break;
      default:
        printf("Encryption=\033[0;35munknown\033[0;0m ");
        break;
    }

    printf("Protocol version: \033[0;35m%d\033[0;0m ", (data[79] & 0x30) >> 4);

    switch ((data[79] & 0xC0) >> 6) {
      case 0:
        printf("Node type=\033[0;35mALPHA\033[0;0m \n");
        break;
      case 1:
        printf("Node type=\033[0;35mBETA\033[0;0m \n");
        break;
      case 2:
        printf("Node type=\033[0;35mGAMMA\033[0;0m \n");
        break;
      default:
        printf("Node type=\033[0;35munknown\033[0;0m \n");
        break;
    }

    printf("Head=\033[0;35m0x%04X \033[0;0m", (data[80] << 8) + data[81]);
    printf("Nickname=\033[0;35m0x%04X\033[0;0m ", (data[82] << 8) + data[83]);
    printf("Class=\033[0;35m0x%04X\033[0;0m ", (data[84] << 8) + data[85]);
    printf("Type=\033[0;35m0x%04X\033[0;0m ", (data[86] << 8) + data[87]);
    printf("Size of data=\033[0;35m%d\033[0;0m \n", data[88]);
    printf("Data: \033[0;35m");
    for (int i = 0; i < data[88]; i++) {
      printf("0x%02X ", data[88 + i]);
    }
    printf("\033[0;0m\n");
  }
  else {
    printf("\033[0;31mPayload is not VSCP\033[0;0m\n");
  }

  // Dump frames raw

  if (bEncrypted) {
    printf("\033[31m");
  }
  else {
    printf("\033[32m");
  }

  int i;
  for (i = 0; i < len; i++) {
    if (i % 16 == 0)
      printf("\n");
    printf("0x%02x, ", data[i]);
  }

  printf("\033[0m\n\n");
}

///////////////////////////////////////////////////////////////////////////////
// create_raw_socket
//

int
create_raw_socket(char *dev, struct sock_fprog *bpf)
{
  struct sockaddr_ll sll;
  struct ifreq ifr;
  int fd, ifi, rb, attach_filter;

  bzero(&sll, sizeof(sll));
  bzero(&ifr, sizeof(ifr));

  fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  assert(fd != -1);

  strncpy((char *) ifr.ifr_name, dev, IFNAMSIZ);
  ifi = ioctl(fd, SIOCGIFINDEX, &ifr);
  assert(ifi != -1);

  sll.sll_protocol = htons(ETH_P_ALL);
  sll.sll_family   = PF_PACKET;
  sll.sll_ifindex  = ifr.ifr_ifindex;
  sll.sll_pkttype  = PACKET_OTHERHOST;

  rb = bind(fd, (struct sockaddr *) &sll, sizeof(sll));
  assert(rb != -1);

  attach_filter = setsockopt(fd, SOL_SOCKET, SO_ATTACH_FILTER, bpf, sizeof(*bpf));
  assert(attach_filter != -1);

  return fd;
}

///////////////////////////////////////////////////////////////////////////////
// main
//

int
main(int argc, char **argv)
{
  assert(argc == 2);

  uint8_t buff[MAX_PACKET_LEN] = { 0 };
  int sock_fd;
  char *dev             = argv[1];
  struct sock_fprog bpf = { FILTER_LENGTH, bpfcode };

  sock_fd = create_raw_socket(dev, &bpf); /* Creating the raw socket */

  printf("\n Waiting to receive esp-now frames ........ \n");

  while (1) {
    int len = recvfrom(sock_fd, buff, MAX_PACKET_LEN, MSG_TRUNC, NULL, 0);

    if (len < 0) {
      perror("Socket receive failed or error");
      break;
    }
    else {
      printf("len:%d\n", len);
      print_packet(buff, len);
    }
  }
  close(sock_fd);
  return 0;
}
