/* Author: Maciek Muszkowski 
 * 
 * This is a simple ping implementation for Linux.
 * It will work ONLY on kernels 3.x+ and you need
 * to add allowed groups to /proc/sys/net/ipv4/ping_group_range */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <unistd.h>

#define ECHO_REQ_PACKET_SIZE  64
/// ICMP echo request packet
struct icmp_echo_req {
    struct icmphdr hdr;
    char data[ECHO_REQ_PACKET_SIZE-sizeof(struct icmphdr)];
};

// number incremented with every echo request packet send
static short seq = 1;
// packet Time-To-Live - max possible
static const int TTL = 255;

/// 1s complementary checksum
unsigned short ping_checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum=0;
    unsigned short result;

    for ( sum = 0; len > 1; len -= 2 )
        sum += *buf++;
    if ( len == 1 )
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

#define EPING_HOST  -1LL // Unresolvable hostname
#define EPING_SOCK  -2LL // Socket creation failed
#define EPING_TTL   -3LL // Setting TTL failed
#define EPING_SETTO -4LL // Setting timeout failed
#define EPING_SEND  -5LL // Sending echo request failed
#define EPING_DST   -6LL // Destination unreachable
#define EPING_TIME  -7LL // Timeout
#define EPING_UNK   -8LL // Unknown error

/// @return value < 0 on error, response time in ms on success
extern long long ping(const char* adress, int timeout_s) {
    int i, sd;
    short pid, sent_seq;
    struct icmp_echo_req pckt;
    char inbuf[192];
    struct sockaddr_in r_addr;
    struct hostent* hname;
    struct sockaddr_in addr_ping, *addr;
    struct timeval timeout, start, end;
    struct iphdr* iph;
    struct icmphdr* icmph;
    socklen_t slen;
    int rlen;

    // resolve hostname
    hname = gethostbyname(adress);
    if(!hname)
        return EPING_HOST;

    // create socket
    if(((sd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) &&
       ((sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP)) < 0))
        return EPING_SOCK;

    // set TTL
    if (setsockopt(sd, SOL_IP, IP_TTL, &TTL, sizeof(TTL)) != 0) {
        close(sd);
        return EPING_TTL;
    }

    // set timeout in secs (do not use secs - BUGGY)
    timeout.tv_sec = timeout_s;
    timeout.tv_usec = 0;
    if(setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
        close(sd);
        return EPING_SETTO;
    }
    
    // set IP address to ping
    memset(&addr_ping, 0, sizeof(addr_ping));
    addr_ping.sin_family = hname->h_addrtype;
    addr_ping.sin_port = 0;
    memcpy(&addr_ping.sin_addr, hname->h_addr, hname->h_length);
    addr = &addr_ping;

    // prepare echo request packet
    pid = getpid() & 0xFFFF;
    memset(&pckt, 0, sizeof(pckt));
    pckt.hdr.type = ICMP_ECHO;
    pckt.hdr.un.echo.id = pid;
    for ( i = 0; i < sizeof(pckt.data)-1; i++ )
        pckt.data[i] = i+'0';
    pckt.data[i] = 0;
    pckt.hdr.un.echo.sequence = (sent_seq = seq++);
    pckt.hdr.checksum = ping_checksum(&pckt, sizeof(pckt));

    // send echo request
    gettimeofday(&start, NULL);
    if(sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0) {
        close(sd);
        return EPING_SEND;
    }

    // receive response (if any)
    slen = sizeof(r_addr);
    while((rlen = recvfrom(sd, &inbuf, sizeof(pckt), 0, (struct sockaddr*)&r_addr, &slen)) > 0) {
        gettimeofday(&end, NULL);
        
        // skip malformed
        if(rlen != ECHO_REQ_PACKET_SIZE) continue;
        
        // parse response
        iph = (struct iphdr*)inbuf;
        icmph = (struct icmphdr*)(inbuf + (iph->ihl << 2));

        // skip the ones we didn't send
        if(icmph->un.echo.id != pid || icmph->un.echo.sequence != sent_seq) continue;

        close(sd);
        switch(icmph->type) {
            case ICMP_ECHOREPLY: return 1000000 * (end.tv_sec - start.tv_sec) +
                                                  (end.tv_usec - start.tv_usec);
            case ICMP_DEST_UNREACH:return EPING_DST;
            case ICMP_TIME_EXCEEDED: return EPING_TIME;
            default: return EPING_UNK;
        }
    }

    // no response in specified time
    close(sd);
    return EPING_TIME;
}

