/**
 * @file frame_test.c
 * @brief F4 gate app: the framed host protocol (firmware track).
 *
 * Receives frames, answers them, and logs every decoded event in a readable form
 * so the gate can assert on the transcript rather than on binary wire bytes:
 *
 *   [FRAME] rx ok  seq=<n> cmd=<n> len=<n>                    (valid, first time)
 *   [FRAME] rx dup seq=<n> cmd=<n> len=<n>                    (valid repeat)
 *   [FRAME] rx ok  seq=<n> cmd=<n> len=<n> hex=<payload hex>  (when len > 0)
 *   [FRAME] tx echo|stats|nak seq=<n>
 *   [TEST] frames ok=<n> bad_crc=<n> bad_len=<n> resync=<n> dup=<n> tx=<n>
 *
 * The counters are the protocol's own evidence: a corrupt frame must show up as
 * bad_crc with a NAK on the wire (counted in tx), never as a delivered frame.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/uart_rx.h"
#include "../net/frame.h"
#include <omni/stdio.h>

#define MTIME_LO (*(volatile uint32_t *)0x0200BFF8u)
#define IDLE_STOP 2000000u          /* 40 ms with nothing to do -> report */

static void log_frame(const frame_t *f)
{
    if (f->len) {
        printf("[FRAME] rx %s seq=%u cmd=%u len=%u hex=",
               f->dup ? "dup" : "ok", f->seq, f->cmd, f->len);
        for (unsigned i = 0; i < f->len; i++) printf("%02x", f->payload[i]);
        printf("\r\n");
    } else {
        printf("[FRAME] rx %s seq=%u cmd=%u len=0\r\n",
               f->dup ? "dup" : "ok", f->seq, f->cmd);
    }
}

static void handle(const frame_t *f)
{
    log_frame(f);

    if (f->dup) {
        /* A repeat of the last accepted seq means our answer went missing:
         * answer again, do nothing else. Idempotence is the point. */
        frame_send(f->seq, f->cmd, f->payload, f->len);
        printf("[FRAME] tx repeat seq=%u\r\n", f->seq);
        return;
    }

    switch (f->cmd) {
    case FRAME_CMD_ECHO:
        printf("[FRAME] tx echo seq=%u\r\n", f->seq);
        frame_send(f->seq, FRAME_CMD_ECHO, f->payload, f->len);
        break;
    case FRAME_CMD_STATS: {
        uint8_t p[5];
        p[0] = (uint8_t)frame_rx_ok();
        p[1] = (uint8_t)frame_rx_bad_crc();
        p[2] = (uint8_t)frame_rx_bad_len();
        p[3] = (uint8_t)frame_rx_resync();
        p[4] = (uint8_t)frame_rx_dup();
        printf("[FRAME] tx stats seq=%u\r\n", f->seq);
        frame_send(f->seq, FRAME_CMD_STATS, p, 5);
        break;
    }
    default:
        printf("[FRAME] tx nak seq=%u (unknown cmd=%u)\r\n", f->seq, f->cmd);
        frame_send_nak(f->seq);
        break;
    }
}

void app_main(void)
{
    frame_t f;
    uint32_t last = MTIME_LO;

    uart_init();
    uart_rx_init();
    frame_init();
    printf("[TEST] READY\r\n");          /* TB_START_MARK: the host feeds after this */

    for (;;) {
        if (frame_poll(&f)) {
            last = MTIME_LO;
            handle(&f);
            continue;
        }
        if ((uint32_t)(MTIME_LO - last) > IDLE_STOP) break;   /* session over */
    }

    printf("[TEST] frames ok=%u bad_crc=%u bad_len=%u resync=%u dup=%u tx=%u\r\n",
           frame_rx_ok(), frame_rx_bad_crc(), frame_rx_bad_len(),
           frame_rx_resync(), frame_rx_dup(), frame_tx_frames());
    printf("[TEST] SUMMARY 1/1\r\n");
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
