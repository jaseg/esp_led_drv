#ifndef __LED_CONTROLLER_H__
#define __LED_CONTROLLER_H__

#define LED_NCH 2

struct channel {
    union {
        struct {
            uint16_t r, g, b, w;
        };
        uint16_t rgbw[4];
    };
};

struct channel_delta {
    union {
        struct {
            int r, g, b, w;
        };
        int rgbw[4];
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

void hsv_to_rgb(int *r, int *g, int *b, int h, int s, int v);

void led_ctrl_init(void);
void send_packet_formatted(uint8_t *buf, int len);
void send_framebuf(struct framebuf *fb);
void fade_timer_cb(void *arg);

enum fade_curve {
    FADE_LINEAR
};

void set_channel(int ch, struct channel value);
void fade_channel(int ch, int duration_ms, enum fade_curve curve, struct channel value);

extern struct framebuf current_framebuf;

#endif
