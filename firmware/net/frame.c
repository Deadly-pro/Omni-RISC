/* Omni-RISC APU - framed host protocol (firmware track, F4).
 * Spec: firmware/net/frame.h.
 */
#include "../drivers/uart.h"        /* uart_putc, for the transmit side */
#include "../drivers/uart_rx.h"
#include "frame.h"

/* body = len seq cmd payload[len] crc_hi crc_lo */
#define BODY_MAX (3u + FRAME_MAX_PAYLOAD + 2u)

static unsigned rx_ok, rx_bad_crc, rx_bad_len, rx_resync, rx_dup, tx_frames;
static uint8_t  body[BODY_MAX];
static unsigned blen;                 /* bytes collected into body[] */
static int      in_frame;
static int      esc;                  /* last byte was 0x7D */
static int      last_seq, have_last;
static frame_t  delivered;            /* scratch for a parsed frame */

uint16_t frame_crc16(const uint8_t *p, unsigned n)
{
    uint16_t crc = 0xFFFFu;
    for (unsigned i = 0; i < n; i++) {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000u) ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
    }
    return crc;
}

void frame_init(void)
{
    rx_ok = rx_bad_crc = rx_bad_len = rx_resync = rx_dup = tx_frames = 0;
    blen = 0;
    in_frame = 0;
    esc = 0;
    last_seq = 0;
    have_last = 0;
}

unsigned frame_rx_ok(void)     { return rx_ok; }
unsigned frame_rx_bad_crc(void) { return rx_bad_crc; }
unsigned frame_rx_bad_len(void) { return rx_bad_len; }
unsigned frame_rx_resync(void) { return rx_resync; }
unsigned frame_rx_dup(void)    { return rx_dup; }
unsigned frame_tx_frames(void) { return tx_frames; }

/* ------------------------------------------------------------- sending ---- */
static void tx_raw(uint8_t b)
{
    uart_putc((char)b);
}

static void tx_byte(uint8_t b)
{
    if (b == FRAME_FLAG || b == FRAME_ESC) {
        tx_raw(FRAME_ESC);
        tx_raw((uint8_t)(b ^ 0x20u));
    } else {
        tx_raw(b);
    }
}

void frame_send(uint8_t seq, uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t hdr[3];
    uint16_t crc;

    hdr[0] = len;
    hdr[1] = seq;
    hdr[2] = cmd;

    tx_raw(FRAME_FLAG);
    for (unsigned i = 0; i < 3; i++) tx_byte(hdr[i]);
    for (unsigned i = 0; i < len; i++) tx_byte(payload[i]);

    crc = 0xFFFFu;                       /* CRC over len..payload */
    for (unsigned i = 0; i < 3; i++) {
        crc ^= (uint16_t)((uint16_t)hdr[i] << 8);
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000u) ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
    }
    for (unsigned i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)payload[i] << 8);
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000u) ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
    }
    tx_byte((uint8_t)(crc >> 8));
    tx_byte((uint8_t)(crc & 0xFFu));
    tx_raw(FRAME_FLAG);
    tx_frames++;
}

void frame_send_ack(uint8_t seq) { frame_send(seq, 0x00u, 0, 0); }

void frame_send_nak(uint8_t seq)
{
    static const uint8_t nak = 0xFFu;
    frame_send(seq, 0x00u, &nak, 1);
}

/* ----------------------------------------------------------- receiving ---- */
/* Evaluate what is in body[] once it looks complete. Returns 1 on a good frame. */
static int body_complete(void)
{
    unsigned want;
    uint8_t  len, seq;
    uint16_t crc, want_crc;

    if (blen < 1u) return 0;
    len = body[0];
    if (len > FRAME_MAX_PAYLOAD) {
        /* the declared length is impossible: reject as soon as we know, and keep
         * discarding until the next flag (in_frame stays set) */
        return 0;
    }
    want = 3u + len + 2u;
    if (blen < want) return 0;

    seq = body[1];
    crc = frame_crc16(body, 3u + len);
    want_crc = (uint16_t)(((uint16_t)body[3u + len] << 8) | body[3u + len + 1u]);

    if (crc != want_crc) {
        rx_bad_crc++;
        frame_send_nak(seq);
        blen = 0;
        in_frame = 0;
        return 0;
    }

    delivered.seq = seq;
    delivered.cmd = body[2];
    delivered.len = len;
    for (unsigned i = 0; i < len; i++) delivered.payload[i] = body[3u + i];
    delivered.dup = (have_last && seq == (uint8_t)last_seq) ? 1 : 0;
    if (delivered.dup) rx_dup++;
    last_seq = seq;
    have_last = 1;
    rx_ok++;
    blen = 0;
    in_frame = 0;
    return 1;
}

int frame_poll(frame_t *out)
{
    uart_rx_pump();
    for (;;) {
        int c = uart_rx_getc();
        if (c < 0) break;

        if (esc) {
            /* the byte after 0x7D is the original XOR 0x20; it can never be a
             * flag (0x7E ^ 0x20 == 0x5E), so this branch must run first */
            c = (int)((uint8_t)c ^ 0x20u);
            esc = 0;
        } else if ((uint8_t)c == FRAME_FLAG) {
            /* A flag always (re)starts a frame. Arriving mid-collection means the
             * previous one was truncated, which is a resync, not a lost cause. */
            if (in_frame && blen > 0) rx_resync++;
            blen = 0;
            in_frame = 1;
            continue;
        } else if ((uint8_t)c == FRAME_ESC) {
            esc = 1;
            continue;
        }
        if (!in_frame) continue;                 /* garbage before the first flag */

        if (blen == 1u && body[0] > FRAME_MAX_PAYLOAD) {
            /* already known-bad length: keep eating until the next flag */
            continue;
        }
        if (blen >= BODY_MAX) {                  /* longer than any legal body */
            rx_bad_len++;
            frame_send_nak(0);
            blen = 0;
            in_frame = 0;
            continue;
        }
        body[blen++] = (uint8_t)c;

        if (blen == 1u && body[0] > FRAME_MAX_PAYLOAD) {
            rx_bad_len++;
            frame_send_nak(0);
            /* stay in_frame: the flag that ends this frame will resync us */
            continue;
        }
        if (body_complete()) {
            *out = delivered;
            return 1;
        }
    }
    return 0;
}
