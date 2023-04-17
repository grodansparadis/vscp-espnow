/*
Etienne Arlaud
*/

#include <arpa/inet.h>
#include <assert.h>
#include <linux/filter.h>
#include <linux/if_arp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#define PRIORITY_LVL -20
#define SOCKET_PRIORITY 7

#include "ESPNOW_packet.h"

#define MY_MAC 0xffffffffffff // 0xf81a67b7eb0b
static uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t src_mac[6] = {0xF8, 0x1A, 0x67, 0xb7, 0xEB, 0x0B};
// static uint8_t dest_mac[6] = {0x84, 0xf3, 0xeb, 0x73, 0x55, 0x0d}; //ESP8266
static uint8_t dest_mac[6] = {
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF}; //{0xB4, 0xE6, 0x2D, 0xB5, 0x9F, 0x85};// ESP32

#define MAX_PACKET_LEN 1000

#define FILTER_LENGTH 53 // 34
#define MAC_2_MSBytes(MAC) ((uint64_t)MAC & (uint64_t)0xffff00000000) >> (8 * 4)
#define MAC_4_LSBytes(MAC) (uint64_t) MAC &(((uint64_t)1 << (4 * 8)) - 1)

// generated with tcpdump -i wlp5s0 'type 0 subtype 0xd0 and
// wlan[24:4]=0x7f18fe34 and wlan[32]=221 and wlan[33:4]&0xffffff = 0x18fe34 and
// wlan[37]=0x4 and wlan dst f8:1a:67:b7:eb:0b' -dd
static struct sock_filter bpfcode[FILTER_LENGTH] = {
    {0x30, 0, 0, 0x00000003},   {0x64, 0, 0, 0x00000008},
    {0x7, 0, 0, 0x00000000},    {0x30, 0, 0, 0x00000002},
    {0x4c, 0, 0, 0x00000000},   {0x2, 0, 0, 0x00000000},
    {0x7, 0, 0, 0x00000000},    {0x50, 0, 0, 0x00000000},
    {0x54, 0, 0, 0x000000fc},   {0x15, 0, 42, 0x000000d0},
    {0x40, 0, 0, 0x00000018},   {0x15, 0, 40, 0x7f18fe34},
    {0x50, 0, 0, 0x00000020},   {0x15, 0, 38, 0x000000dd},
    {0x40, 0, 0, 0x00000021},   {0x54, 0, 0, 0x00ffffff},
    {0x15, 0, 35, 0x0018fe34},  {0x50, 0, 0, 0x00000025},
    {0x15, 0, 33, 0x00000004},  {0x50, 0, 0, 0x00000000},
    {0x45, 31, 0, 0x00000004},  {0x45, 0, 21, 0x00000008},
    {0x50, 0, 0, 0x00000001},   {0x45, 0, 4, 0x00000001},
    {0x40, 0, 0, 0x00000012},   {0x15, 0, 26, 0xffffffff},
    {0x48, 0, 0, 0x00000010},   {0x15, 4, 24, 0x0000ffff},
    {0x40, 0, 0, 0x00000006},   {0x15, 0, 22, 0xffffffff},
    {0x48, 0, 0, 0x00000004},   {0x15, 0, 20, 0x0000ffff},
    {0x50, 0, 0, 0x00000001},   {0x45, 0, 13, 0x00000002},
    {0x45, 0, 4, 0x00000001},   {0x40, 0, 0, 0x0000001a},
    {0x15, 0, 15, 0x2db59f85},  {0x48, 0, 0, 0x00000018},
    {0x15, 12, 13, 0x0000b4e6}, {0x40, 0, 0, 0x00000012},
    {0x15, 0, 11, 0x2db59f85},  {0x48, 0, 0, 0x00000010},
    {0x15, 8, 9, 0x0000b4e6},   {0x40, 0, 0, 0x00000006},
    {0x15, 0, 7, 0xffffffff},   {0x48, 0, 0, 0x00000004},
    {0x15, 0, 5, 0x0000ffff},   {0x40, 0, 0, 0x0000000c},
    {0x15, 0, 3, 0x2db59f85},   {0x48, 0, 0, 0x0000000a},
    {0x15, 0, 1, 0x0000b4e6},   {0x6, 0, 0, 0x00040000},
    {0x6, 0, 0, 0x00000000},

    /*
      { 0x30, 0, 0, 0x00000003 },
      { 0x64, 0, 0, 0x00000008 },
      { 0x7, 0, 0, 0x00000000 },
      { 0x30, 0, 0, 0x00000002 },
      { 0x4c, 0, 0, 0x00000000 },
      { 0x2, 0, 0, 0x00000000 },
      { 0x7, 0, 0, 0x00000000 },
      { 0x50, 0, 0, 0x00000000 },
      { 0x54, 0, 0, 0x000000fc },
      { 0x15, 0, 23, 0x000000d0 },
      { 0x40, 0, 0, 0x00000018 },
      { 0x15, 0, 21, 0x7f18fe34 },
      { 0x50, 0, 0, 0x00000020 },
      { 0x15, 0, 19, 0x000000dd },
      { 0x40, 0, 0, 0x00000021 },
      { 0x54, 0, 0, 0x00ffffff },
      { 0x15, 0, 16, 0x0018fe34 },
      { 0x50, 0, 0, 0x00000025 },
      { 0x15, 0, 14, 0x00000004 },
      { 0x50, 0, 0, 0x00000000 },
      { 0x45, 12, 0, 0x00000004 },
      { 0x45, 0, 6, 0x00000008 },
      { 0x50, 0, 0, 0x00000001 },
      { 0x45, 0, 4, 0x00000001 },
      { 0x40, 0, 0, 0x00000012 },
      { 0x15, 0, 7, 0x67b7eb0b },
      { 0x48, 0, 0, 0x00000010 },
      { 0x15, 4, 5, 0x0000f81a },
      { 0x40, 0, 0, 0x00000006 },
      { 0x15, 0, 3, MAC_4_LSBytes(MY_MAC) },
      { 0x48, 0, 0, 0x00000004 },
      { 0x15, 0, 1, MAC_2_MSBytes(MY_MAC) },
      { 0x6, 0, 0, 0x00040000 },
      { 0x6, 0, 0, 0x00000000 },
    */
};

ESPNOW_packet echo_packet;
uint8_t raw_bytes[400];

///////////////////////////////////////////////////////////////////////////////
// create_raw_socket
//

int
create_raw_socket(char *dev, struct sock_fprog *bpf)
{
  struct sockaddr_ll sll;
  struct ifreq ifr;
  int fd, ifi, rb, attach_filter;
  int priority;
  int priority_errno; // Set priority errno

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

  priority = SOCKET_PRIORITY;
  priority_errno =
  setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority));
  assert(priority_errno == 0);

  return fd;
}

int main(int argc, char **argv) {
  assert(argc == 2);

  nice(PRIORITY_LVL);

  char *dev = argv[1];

  int packets_received = 0;

  uint8_t buff[MAX_PACKET_LEN] = {0};
  int sock_fd = -1;
  int s32_res = -1;

  struct sock_fprog bpf = {FILTER_LENGTH, bpfcode};

  sock_fd = create_raw_socket(dev, &bpf); /* Creating the raw socket */

  if (sock_fd < 0) {
    perror("Could not create the socket");
    goto LABEL_CLEAN_EXIT;
  } else {
    printf("Socket created\n");
  }

  fflush(stdout);

  sleep(1);

  // init answer packet
  init_ESPNOW_packet(&echo_packet);
  memcpy(echo_packet.wlan.da, dest_mac, sizeof(uint8_t) * 6);
  memcpy(echo_packet.wlan.sa, src_mac, sizeof(uint8_t) * 6);
  memcpy(echo_packet.wlan.bssid, dest_mac, sizeof(uint8_t) * 6);

  while (1) {
    int len = recvfrom(sock_fd, buff, MAX_PACKET_LEN, MSG_TRUNC, NULL, 0);

    if (len < 0) {
      perror("Socket receive failed or error");
      goto LABEL_CLEAN_EXIT;
    } else if (len > 77) {
      // printf("Receive packet number : %d\n", ++packets_received);
      // print_raw_packet(buff, len);

      // generate echo
      memcpy(echo_packet.wlan.actionframe.content.payload, buff + 77, 16);
      int mypacket_len = packet_to_bytes(raw_bytes, 400, echo_packet);
      s32_res = sendto(sock_fd, raw_bytes, mypacket_len, 0, NULL, 0);

      if (-1 == s32_res) {
        perror("Socket send failed");
        goto LABEL_CLEAN_EXIT;
      } else {
        // printf("Echo sent\n\n\n");
      }
    }
  }

LABEL_CLEAN_EXIT:
  if (sock_fd > 0) {
    close(sock_fd);
  }

  printf("***** Raw Socket test- end\n");

  return EXIT_SUCCESS;
}
