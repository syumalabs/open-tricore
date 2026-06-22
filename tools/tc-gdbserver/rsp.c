/*
 * RSP transport, see rsp.h.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#include "rsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

static int read_byte(int fd, unsigned char *c) {
    ssize_t n = read(fd, c, 1);
    return n == 1 ? 0 : -1;
}

static int write_all(int fd, const char *p, size_t n) {
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w <= 0) return -1;
        p += w; n -= (size_t)w;
    }
    return 0;
}

static const char hexd[] = "0123456789abcdef";
static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int rsp_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* localhost only */
    a.sin_port = htons((unsigned short)port);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) { close(fd); return -1; }
    if (listen(fd, 1) < 0) { close(fd); return -1; }
    return fd;
}

int rsp_accept(int listen_fd) {
    int fd = accept(listen_fd, NULL, NULL);
    if (fd < 0) return -1;
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)); /* low latency for the chatty protocol */
    return fd;
}

int rsp_get_packet(int fd, char *buf, size_t bufsz) {
    for (;;) {
        unsigned char c;
        /* find the start of a packet, honouring an out of band interrupt */
        do {
            if (read_byte(fd, &c)) return -1;
            if (c == 0x03) return -2;
        } while (c != '$');

        size_t len = 0;
        unsigned char sum = 0;
        int ok = 1;
        for (;;) {
            if (read_byte(fd, &c)) return -1;
            if (c == '#') break;
            if (len + 1 >= bufsz) { ok = 0; }      /* overflow, will nak */
            else { buf[len++] = (char)c; sum += c; }
        }
        unsigned char ch[2];
        if (read_byte(fd, &ch[0]) || read_byte(fd, &ch[1])) return -1;
        int hi = hexval(ch[0]), lo = hexval(ch[1]);
        unsigned char want = (unsigned char)((hi << 4) | lo);

        if (ok && hi >= 0 && lo >= 0 && want == sum) {
            if (write_all(fd, "+", 1)) return -1;
            buf[len] = '\0';
            return (int)len;
        }
        if (write_all(fd, "-", 1)) return -1;       /* bad packet, ask for resend */
    }
}

int rsp_put_packet(int fd, const char *data, size_t len) {
    char *pkt = NULL;
    size_t cap = len + 4;
    char stackbuf[512];
    char *p = (cap <= sizeof(stackbuf)) ? stackbuf : (pkt = (char *)malloc(cap));
    if (!p) return -1;

    for (;;) {
        size_t k = 0;
        unsigned char sum = 0;
        p[k++] = '$';
        for (size_t i = 0; i < len; i++) { p[k++] = data[i]; sum += (unsigned char)data[i]; }
        p[k++] = '#';
        p[k++] = hexd[(sum >> 4) & 0xF];
        p[k++] = hexd[sum & 0xF];
        if (write_all(fd, p, k)) { free(pkt); return -1; }

        unsigned char ack;
        if (read_byte(fd, &ack)) { free(pkt); return -1; }
        if (ack == '+') { free(pkt); return 0; }
        if (ack == '-') continue;                  /* retransmit */
        /* any other byte, treat as ack to stay robust */
        free(pkt); return 0;
    }
}

int rsp_put_str(int fd, const char *s) { return rsp_put_packet(fd, s, strlen(s)); }
