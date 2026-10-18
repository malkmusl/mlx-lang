#ifndef _MLXCC_NETINET_IN_H
#define _MLXCC_NETINET_IN_H
#include <stdint.h>
#include <sys/socket.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
struct in_addr { in_addr_t s_addr; };
struct in6_addr { union { uint8_t __u6_addr8[16]; uint16_t __u6_addr16[8]; uint32_t __u6_addr32[4]; } __in6_u; };
#define s6_addr __in6_u.__u6_addr8
#define s6_addr16 __in6_u.__u6_addr16
#define s6_addr32 __in6_u.__u6_addr32
struct sockaddr_in { sa_family_t sin_family; in_port_t sin_port; struct in_addr sin_addr; unsigned char sin_zero[8]; };
struct sockaddr_in6 { sa_family_t sin6_family; in_port_t sin6_port; uint32_t sin6_flowinfo; struct in6_addr sin6_addr; uint32_t sin6_scope_id; };
#define IPPROTO_IP 0
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define IPPROTO_IPV6 41
#define IPPROTO_SCTP 132
#define IPPROTO_RAW 255
#define INADDR_ANY ((in_addr_t)0x00000000)
#define INADDR_BROADCAST ((in_addr_t)0xffffffff)
#define INADDR_NONE ((in_addr_t)0xffffffff)
#define INADDR_LOOPBACK ((in_addr_t)0x7f000001)
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46
static inline uint16_t __mlxcc_bswap16(uint16_t x) { return (uint16_t)((x << 8) | (x >> 8)); }
static inline uint32_t __mlxcc_bswap32(uint32_t x) { return (x << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | (x >> 24); }
static inline uint32_t htonl(uint32_t x) { return __mlxcc_bswap32(x); }
static inline uint16_t htons(uint16_t x) { return __mlxcc_bswap16(x); }
static inline uint32_t ntohl(uint32_t x) { return __mlxcc_bswap32(x); }
static inline uint16_t ntohs(uint16_t x) { return __mlxcc_bswap16(x); }
#ifdef __cplusplus
}
#endif
#endif
