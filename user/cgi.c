/*
Some random cgi routines. Used in the LED example and the page that returns the entire
flash as a binary. Also handles the hit counter on the main page.
*/

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>
#include "cgi.h"
#include "io.h"
#include "led_controller.h"
#include "re1.5.h"

int ICACHE_FLASH_ATTR cgiRgbw(HttpdConnData *connData) {
	int len, sc=200;
    char buf[512];

    const char *msg;

    if (connData->conn == NULL) /* Connection aborted */
        return HTTPD_CGI_DONE;

    const char *re_str = "^(rgb:|rgbw:|hsv:)?([0-9]+),([0-9]+),([0-9]+)(,([0-9]+))?(;fade:([0-9]+m?)s(:linear)?)?$";
    int size = re1_5_sizecode(re_str);
    if (size == -1) {
        msg = "regex error\n"; sc = 500;
        goto errout;
    }
    char channel_arg_prog_char[512];
    struct ByteProg *channel_arg_prog = (struct ByteProg *)channel_arg_prog_char;
    re1_5_compilecode(channel_arg_prog, re_str);
    struct cap {
        char *start, *end;
    };
    union {
        struct {
            struct cap whole, colorsys, v0, v1, v2, v3_cont, v3, fade_cont, fade_duration, fade_curve;
        };
        char *caps[0];
    } caps = {.whole={0}};

    char *channel_names[2] = {"ch0", "ch1"};
    for (int i=0; i<2; i++) {
        len = httpdFindArg(connData->post->buff, channel_names[i], buf, sizeof(buf));
        if (len < 0)
            continue;

        /* force-zero-terminate to compensate lack of strnchr */
        buf[sizeof(buf)-1] = 0;

        Subject subj = {
            .begin = buf,
            .end = buf+len
        };
        /* Be careful with the weird caps numbering. Each capture group requires *two* capture pointers, one for its
         * start and one for its end. */
        int res = re1_5_recursiveloopprog(channel_arg_prog, &subj, (const char **)&caps, sizeof(caps)/sizeof(char *), 1);
        if (res == 0) {
            msg = "Invalid parameter syntax\n"; sc = 400;
            goto errout;
        }

        /*
        for (int k=0; k<sizeof(caps)/sizeof(struct cap); k++) {
            size_t l = caps.caps[k*2+1]-caps.caps[k*2];
            char baf[32];
            os_sprintf(baf, "Capture %d/%d len %u: ", k, sizeof(caps)/sizeof(struct cap), l);
            httpdSend(connData, baf, -1);
            httpdSend(connData, caps.caps[k*2], l);
            httpdSend(connData, "\n", -1);
        }
        */

        int v0 = atoi(caps.v0.start),
            v1 = atoi(caps.v1.start),
            v2 = atoi(caps.v2.start),
            v3 = 0;

        if (v0 > 0xffff || v1 > 0xffff || v2 > 0xffff) {
            msg = "Channel value too large\n"; sc = 400;
            goto errout;
        }

        if (caps.colorsys.start == caps.colorsys.end || !strncmp(caps.colorsys.start, "rgbw", 4)) {
            if (!caps.v3.start) {
                msg = "Not enough channels sent\n"; sc = 400;
                goto errout;
            }

            v3 = atoi(caps.v3.start);
        } else {
            if (strncmp(caps.colorsys.start, "hsv", 3)) {
                msg = "Unsupported color space\n"; sc = 400;
                goto errout;
            }

            if (caps.v3.start) {
                msg = "Too many channels sent\n"; sc = 400;
                goto errout;
            }

            hsv_to_rgb(&v0, &v1, &v2, v0, v1, v2);
        }

        if (v3 > 0xffff) {
            msg = "Channel value too large\n"; sc = 400;
            goto errout;
        }

        struct channel ch = (struct channel) {
            .r = v0, .g=v1, .b=v2, .w=v3
        };

        if (caps.fade_cont.start) {
            int dur_ms = atoi(caps.fade_duration.start);
            if (caps.fade_duration.end[-1] != 'm')
                dur_ms *= 1000;
            fade_channel(i, dur_ms, FADE_LINEAR, ch);
        } else {
            /* Note that this will clobber the first channel if there is an error in the second channel. But that's ok. */
            set_channel(i, ch);
        }
    }

    char *p = buf;
    *p++ = '[';
    for (int i=0; i<LED_NCH; i++) {
        if (i > 0)
            *p++ = ',';
        struct channel *ch = &current_framebuf.chs[i];
        p += os_sprintf(p, "[%d,%d,%d,%d]", ch->r, ch->g, ch->b, ch->w);
    }
    *p++ = ']';
    *p++ = '\n';
    *p++ = '\0';

    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
    httpdSend(connData, buf, -1);
    return HTTPD_CGI_DONE;

errout:
    httpdStartResponse(connData, sc);
    httpdEndHeaders(connData);
    httpdSend(connData, msg, -1);
    return HTTPD_CGI_DONE;
}

//cause I can't be bothered to write an ioGetLed()
static char currLedState=0;

//Cgi that turns the LED on or off according to the 'led' param in the POST data
int ICACHE_FLASH_ATTR cgiLed(HttpdConnData *connData) {
	int len;
	char buff[1024];
	
	if (connData->conn==NULL) {
		//Connection aborted. Clean up.
		return HTTPD_CGI_DONE;
	}

	len=httpdFindArg(connData->post->buff, "led", buff, sizeof(buff));
	if (len!=0) {
		currLedState=atoi(buff);
		ioLed(currLedState);
	}

	httpdRedirect(connData, "led.tpl");
	return HTTPD_CGI_DONE;
}

//Template code for the led page.
int ICACHE_FLASH_ATTR tplLed(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	os_strcpy(buff, "Unknown");
	if (os_strcmp(token, "ledstate")==0) {
		if (currLedState) {
			os_strcpy(buff, "on");
		} else {
			os_strcpy(buff, "off");
		}
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}

static long hitCounter=0;

//Template code for the counter on the index page.
int ICACHE_FLASH_ATTR tplCounter(HttpdConnData *connData, char *token, void **arg) {
	char buff[128];
	if (token==NULL) return HTTPD_CGI_DONE;

	if (os_strcmp(token, "counter")==0) {
		hitCounter++;
		os_sprintf(buff, "%ld", hitCounter);
	}
	httpdSend(connData, buff, -1);
	return HTTPD_CGI_DONE;
}
