/* Omni-RISC APU - framed host protocol (firmware track, F4).
 *
 * The wire format, chosen so that garbage on the link cannot be mistaken for a
 * frame and a corrupted frame can never be delivered:
 *
 *   FLAG(0x7E) body FLAG(0x7E)
 *   body = len seq cmd payload[len] crc_hi crc_lo        (CRC-16/CCITT-FALSE)
 *
 * - 0x7E and 0x7D inside the body are escaped as 0x7D, byte^0x20 (SLIP-style),
 *   which is what lets a receiver resynchronise: any 0x7E starts a new frame.
 * - len is the payload length (0..FRAME_MAX_PAYLOAD). A frame declaring more is
 *   rejected with a NAK instead of being executed.
 * - CRC is over the body bytes len..payload, high byte first. Check value:
 *   crc16("123456789") == 0x29B1.
 *
 * The receiver never executes a frame it could not validate. Retransmission is
 * the *sender's* job (that is what the ACK/NAK pair is for), so this side only
 * answers.
 */
#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>

#define FRAME_FLAG 0x7Eu
#define FRAME_ESC  0x7Du
#define FRAME_MAX_PAYLOAD 32u

/* commands */
#define FRAME_CMD_ECHO  0x01u   /* reply with the same payload */
#define FRAME_CMD_STATS 0x02u   /* reply with the counters below */

typedef struct {
    uint8_t seq;
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[FRAME_MAX_PAYLOAD];
    int     dup;                /* seq repeated the last accepted frame */
} frame_t;

uint16_t frame_crc16(const uint8_t *p, unsigned n);   /* exported for the tests */

void frame_init(void);
/* Pull from the receive path and run the state machine. Returns 1 when a frame
 * survived length + CRC validation (then *out is filled), 0 otherwise. */
int  frame_poll(frame_t *out);

/* Sending: escapes the body, appends the CRC, wraps in flags. */
void frame_send(uint8_t seq, uint8_t cmd, const uint8_t *payload, uint8_t len);
void frame_send_ack(uint8_t seq);
void frame_send_nak(uint8_t seq);

/* Counters (also what CMD_STATS reports) */
unsigned frame_rx_ok(void);
unsigned frame_rx_bad_crc(void);
unsigned frame_rx_bad_len(void);
unsigned frame_rx_resync(void);     /* a flag arrived mid-frame */
unsigned frame_rx_dup(void);
unsigned frame_tx_frames(void);

#endif /* FRAME_H */
