#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Set NO_SYS to 1 for a bare-metal system
#define NO_SYS                          1

// Memory options
#define MEM_LIBC_MALLOC                 1
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (4 * 1024)
#define MEMP_NUM_TCP_SEG                32
#define MEMP_NUM_ARP_QUEUE              10
#define PBUF_POOL_SIZE                  24
#define LWIP_ARP                        1
#define LWIP_TCP                        1
#define LWIP_ICMP                       1
#define LWIP_DHCP                       1
#define LWIP_DNS                        1
#define LWIP_DHCP_SERVER                1

// TCP options
#define TCP_MSS                         1460
#define TCP_WND                         (8 * TCP_MSS)
#define TCP_SND_BUF                     (8 * TCP_MSS)
#define TCP_SND_QUEUELEN                ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

// HTTPD options
#define LWIP_HTTPD                      1
#define LWIP_HTTPD_CGI                  1
#define LWIP_HTTPD_SSI                  1
#define LWIP_HTTPD_SSI_INCLUDE_TAG      0
#define HTTPD_FSDATA_FILE               "html/fsdata.c" // We will create this later

// Platform-specific options
#define LWIP_RAND()                     ((u32_t)rand())

// Checksum options - delegate to hardware if possible
#define CHECKSUM_GEN_IP                 0
#define CHECKSUM_GEN_UDP                0
#define CHECKSUM_GEN_TCP                0
#define CHECKSUM_CHECK_IP               0
#define CHECKSUM_CHECK_UDP              0
#define CHECKSUM_CHECK_TCP              0
#define CHECKSUM_GEN_ICMP               0

#endif /* _LWIPOPTS_H */
