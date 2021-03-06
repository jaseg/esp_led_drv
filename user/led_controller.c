
#include <esp8266.h>
#include <uart_hw.h>
#include <math.h>

#include "led_controller.h"
#include "gammatable.h"

#define NCH LED_NCH

struct framebuf current_framebuf = {.ch1.rgbw={0}};
static struct channel_delta fade_delta[NCH];
static struct channel fade_start[NCH];
static enum fade_curve fade_curve[NCH];
static uint32_t fade_start_time[NCH];
static uint32_t fade_duration[NCH];
static ETSTimer fade_timer;

static void ICACHE_FLASH_ATTR tx_char(char c) {
	//Wait until there is room in the FIFO
	while (((READ_PERI_REG(UART_STATUS(0))>>UART_TXFIFO_CNT_S)&UART_TXFIFO_CNT)>=126) ;
	//Send the character
	WRITE_PERI_REG(UART_FIFO(0), c);
}

void ICACHE_FLASH_ATTR send_packet_formatted(uint8_t *buf, int len) {
    uint8_t *p=buf, *q=buf, *end=buf+len;
    do {
        while (*q && q!=end)
            q++;
        tx_char(q-p+1);
        while (*p && p!=end)
            tx_char(*p++);
        p++, q++;
    } while (p <= end);
    tx_char('\0');
}

void ICACHE_FLASH_ATTR send_framebuf(struct framebuf *fb) {
    uint32_t addr = 0xDEBE10BB;
    send_packet_formatted((uint8_t *)&addr, sizeof(addr));
    send_packet_formatted((uint8_t *)fb, sizeof(*fb));
}

void ICACHE_FLASH_ATTR fade_timer_cb(void *arg) {
    for (int i=0; i<NCH; i++) {
        if (fade_duration[i] != 0) {
            float pos = ((float)((system_get_time()/1000) - fade_start_time[i]))/fade_duration[i];
            if (pos > 1) {
                pos = 1;
                fade_duration[i] = 0;
            }
            current_framebuf.chs[i] = (struct channel){
                .r = fade_start[i].r + pos*fade_delta[i].r,
                .g = fade_start[i].g + pos*fade_delta[i].g,
                .b = fade_start[i].b + pos*fade_delta[i].b,
                .w = fade_start[i].w + pos*fade_delta[i].w
            };
        }
    }
    send_framebuf(&current_framebuf);
}

void led_ctrl_init() {
	//Enable TxD pin
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
	
	//Set baud rate and other serial parameters to 500000,n,8,1
	uart_div_modify(0, UART_CLK_FREQ/BIT_RATE_500000);
	WRITE_PERI_REG(UART_CONF0(0), (STICK_PARITY_DIS)|(ONE_STOP_BIT << UART_STOP_BIT_NUM_S)| \
				(EIGHT_BITS << UART_BIT_NUM_S));

	//Reset tx & rx fifo
	SET_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST|UART_TXFIFO_RST);
	CLEAR_PERI_REG_MASK(UART_CONF0(0), UART_RXFIFO_RST|UART_TXFIFO_RST);
	//Clear pending interrupts
	WRITE_PERI_REG(UART_INT_CLR(0), 0xffff);

	os_timer_disarm(&fade_timer);
	os_timer_setfn(&fade_timer, fade_timer_cb, NULL);
	os_timer_arm(&fade_timer, 20, 1); /* 50 Hz */
}

void set_channel(int ch, struct channel value) {
    fade_duration[ch] = 0;
    current_framebuf.chs[ch] = value;
}

void fade_channel(int ch, int duration_ms, enum fade_curve curve, struct channel value) {
    fade_curve[ch] = curve;
    fade_duration[ch] = duration_ms;
    struct channel *chp = &current_framebuf.chs[ch];
    fade_delta[ch] = (struct channel_delta){
        .r = ((int)value.r) - chp->r,
        .g = ((int)value.g) - chp->g,
        .b = ((int)value.b) - chp->b,
        .w = ((int)value.w) - chp->w
    };
    fade_start[ch] = current_framebuf.chs[ch];
    fade_start_time[ch] = system_get_time()/1000;
}

float fmodf(float a, float b) {
    return (a - b * floor(a / b));
}

void hsv_to_rgb_f(float *r, float *g, float *b, float h, float s, float v) {
    float c = v * s; /* Chroma */
    float hp = fmodf(h / 60.0f, 6);
    float x = c * (1 - fabsf(fmodf(hp, 2) - 1));
    float m = v - c;

    switch ((int)hp) {
    case 0:
        *r = c;
        *g = x;
        *b = 0;
        break;
    case 1:
        *r = x;
        *g = c;
        *b = 0;
        break;
    case 2:
        *r = 0;
        *g = c;
        *b = x;
        break;
    case 3:
        *r = 0;
        *g = x;
        *b = c;
        break;
    case 4:
        *r = x;
        *g = 0;
        *b = c;
        break;
    default:
        *r = c;
        *g = 0;
        *b = x;
        break;
    }

    *r += m;
    *g += m;
    *b += m;
}

void hsv_to_rgb(int *r, int *g, int *b, int h, int s, int v) {
    float fr, fg, fb;
    hsv_to_rgb_f(&fr, &fg, &fb, ((float)h)/65535.0f*360, ((float)s)/65535.0f, ((float)v)/65535.0f);
    *r = (int)((fr+0.5f)*65535);
    *g = (int)((fg+0.5f)*65535);
    *b = (int)((fb+0.5f)*65535);
}

uint16_t apply_gamma(uint16_t val) {
    uint8_t valh = val>>8, vall = val&0xff;
    uint16_t a = gammatable[valh], b = gammatable[valh+1];
    uint16_t d = b - a;

    return a + (((int)vall)*d/256);
}

