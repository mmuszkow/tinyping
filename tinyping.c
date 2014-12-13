/* Author: Maciek Muszkowski 
 * 
 * This is a simple ping implementation for Linux.
 * It will work ONLY on kernels 3.x+ and you need
 * to set allowed groups in /proc/sys/net/ipv4/ping_group_range */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <unistd.h>

#define ECHO_PACKET_SIZE  64

/// ICMP echo request packet, response packet is the same when using SOCK_DGRAM
struct icmp_echo {
    struct icmphdr icmp;
    char data[ECHO_PACKET_SIZE-sizeof(struct icmphdr)];
};

// number incremented with every echo request packet send
static unsigned short seq = 1;
// socket
static int sd = -1;

/// 1s complementary checksum
unsigned short ping_checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
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

#define EPING_SOCK  -1 // Socket creation failed
#define EPING_TTL   -2 // Setting TTL failed
#define EPING_SETTO -3 // Setting timeout failed
#define EPING_HOST  -4 // Unresolvable hostname
#define EPING_SEND  -5 // Sending echo request failed
#define EPING_DST   -6 // Destination unreachable
#define EPING_TIME  -7 // Timeout
#define EPING_UNK   -8 // Unknown error

extern void deinit() {
    if(sd >= 0) {
        close(sd);
        sd = -1;
    }
}

extern int init(int ttl, int timeout_s) {
    struct timeval timeout;

    deinit();

    // create socket
    if(//((sd = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) &&
       ((sd = socket(PF_INET, SOCK_DGRAM, IPPROTO_ICMP)) < 0))
        return EPING_SOCK;

    // set TTL
    if (setsockopt(sd, SOL_IP, IP_TTL, &ttl, sizeof(ttl)) != 0) {
        deinit();
        return EPING_TTL;
    }

    // set timeout in secs (do not use secs - BUGGY)
    timeout.tv_sec = timeout_s;
    timeout.tv_usec = 0;
    if(setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) != 0) {
        deinit();
        return EPING_SETTO;
    }
 
    return 0;
}

/// @return value < 0 on error, response time in ms on success
extern long long ping(const char* hostname) {
    int i;
    unsigned short sent_seq;
    struct icmp_echo req, resp;
    struct sockaddr_in r_addr;
    struct hostent* hname;
    struct sockaddr_in addr_ping, *addr;
    struct timeval start, end;
    socklen_t slen;
    int rlen;

    // not initialized
    if(sd < 0) return EPING_SOCK;

    // resolve hostname
    hname = gethostbyname(hostname);
    if(!hname) return EPING_HOST;

   
    // set IP address to ping
    memset(&addr_ping, 0, sizeof(addr_ping));
    addr_ping.sin_family = hname->h_addrtype;
    addr_ping.sin_port = 0;
    memcpy(&addr_ping.sin_addr, hname->h_addr, hname->h_length);
    addr = &addr_ping;

    // prepare echo request packet
    memset(&req, 0, sizeof(req));
    req.icmp.type = ICMP_ECHO;
    req.icmp.un.echo.id = 0; // SOCK_DGRAM & 0 => id will be set by kernel
    for ( i = 0; i < sizeof(req.data)-1; i++ )
        req.data[i] = i+'0';
    req.data[i] = 0;
    req.icmp.un.echo.sequence = sent_seq = seq++;
    req.icmp.checksum = ping_checksum(&req, sizeof(req));

    // send echo request
    gettimeofday(&start, NULL);
    if(sendto(sd, &req, ECHO_PACKET_SIZE, 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0)
        return EPING_SEND;

    // receive response (if any)
    slen = sizeof(r_addr);
    while((rlen = recvfrom(sd, &resp, ECHO_PACKET_SIZE, 0, (struct sockaddr*)&r_addr, &slen)) > 0) {
        gettimeofday(&end, NULL);

        // skip malformed
        if(rlen != ECHO_PACKET_SIZE) continue;

        // skip the ones we didn't send
        if(resp.icmp.un.echo.sequence != sent_seq) continue;

        switch(resp.icmp.type) {
            case ICMP_ECHOREPLY: return 1000000 * (end.tv_sec - start.tv_sec) +
                                                  (end.tv_usec - start.tv_usec);
            case ICMP_DEST_UNREACH:return EPING_DST;
            case ICMP_TIME_EXCEEDED: return EPING_TIME;
            default: return EPING_UNK;
        }
    }

    // no response in specified time
    return EPING_TIME;
}

//#include <stdio.h>
//int main() {
//    init(255, 2);
//    printf("%lld\n", ping("google.com"));
//    deinit();
//    return 0;
//}

