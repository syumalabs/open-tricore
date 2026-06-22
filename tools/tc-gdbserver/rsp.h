/*
 * RSP, the GDB Remote Serial Protocol transport. Framing, checksums, and
 * acknowledgements over a TCP socket. See rsp.c.
 *
 * Copyright 2026 Syuma Labs. Apache-2.0.
 */

#ifndef RSP_H
#define RSP_H

#include <stddef.h>

/* Open a listening TCP socket on the given port. Returns the fd or -1. */
int rsp_listen(int port);

/* Accept one client connection. Returns the connection fd or -1. */
int rsp_accept(int listen_fd);

/* Receive one packet payload (without the $ and #cc framing) into buf.
   Sends the + acknowledgement on a good checksum, - on a bad one and retries.
   Returns the payload length, 0 for an empty packet, -1 on disconnect or error,
   and -2 if an out of band interrupt (0x03) arrived instead of a packet. */
int rsp_get_packet(int fd, char *buf, size_t bufsz);

/* Send one packet, framed and checksummed, and wait for the + acknowledgement,
   retransmitting on -. Returns 0 on success, -1 on error. */
int rsp_put_packet(int fd, const char *data, size_t len);

/* Convenience, send a NUL terminated string as a packet. */
int rsp_put_str(int fd, const char *s);

#endif
