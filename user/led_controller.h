#ifndef __LED_CONTROLLER_H__
#define __LED_CONTROLLER_H__

struct channel {
    union {
        struct {
            uint16_t r, g, b, w;
        };
        uint16_t rgbw[4];
    };
};

struct framebuf {
    union {
        struct {
            struct channel ch1, ch2;
        };
        struct channel chs[2];
    };
};

void serial_init(void);
void send_packet_formatted(uint8_t *buf, int len);
void send_framebuf(struct framebuf *fb);

#endif
