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

static struct framebuf currentFramebuf = {.ch1.rgbw={0}, .ch2.rgbw={0}};

int ICACHE_FLASH_ATTR cgiRgbw(HttpdConnData *connData) {
	int len;
    char buf[512];

    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);

    const char *msg = "Success\n";

    if (connData->conn == NULL) /* Connection aborted */
        return HTTPD_CGI_DONE;

    httpdSend(connData, "Sizing regex\n", -1);
    const char *re_str = "(rgb:|rgbw:|hsv:)?([0-9]+),([0-9]+),([0-9]+)(,([0-9]+))?(;fade:([0-9]+m?)s)?";
    int size = re1_5_sizecode(re_str);
    if (size == -1) {
        msg = "regex error\n";
        goto out;
    }
    httpdSend(connData, "Compiling regex\n", -1);
    char channel_arg_prog_char[512];
    struct ByteProg *channel_arg_prog = (struct ByteProg *)channel_arg_prog_char;
    re1_5_compilecode(channel_arg_prog, re_str);
    struct cap {
        char *start, *end;
    };
    union {
        struct {
            struct cap whole, colorsys, v0, v1, v2, v3_cont, v3, fade_cont, fade_duration;
        };
        char *caps[0];
    } caps = {.whole={0}};

    httpdSend(connData, "Processing args\n", -1);
    char *channel_names[2] = {"ch1", "ch2"};
    for (int i=0; i<2; i++) {
        httpdSend(connData, "Fetching arg\n", -1);
        len = httpdFindArg(connData->post->buff, channel_names[i], buf, sizeof(buf));
        if (len < 0)
            continue;

        /* force-zero-terminate to compensate lack of strnchr */
        buf[sizeof(buf)-1] = 0;

        httpdSend(connData, "Executing regex\n", -1);
        Subject subj = {
            .begin = buf,
            .end = buf+len
        };
        /* Be careful with the weird caps numbering. Each capture group requires *two* capture pointers, one for its
         * start and one for its end. */
        int res = re1_5_recursiveloopprog(channel_arg_prog, &subj, (const char **)&caps, sizeof(caps)/sizeof(char *), 1);
        httpdSend(connData, "Validating groups\n", -1);
        if (res == 0) {
            msg = "Invalid parameter syntax\n";
            goto out;
        }

        for (int k=0; k<sizeof(caps)/sizeof(struct cap); k++) {
            size_t l = caps.caps[k*2+1]-caps.caps[k*2];
            char baf[32];
            os_sprintf(baf, "Capture %d/%d len %u: ", k, sizeof(caps)/sizeof(struct cap), l);
            httpdSend(connData, baf, -1);
            httpdSend(connData, caps.caps[k*2], l);
            httpdSend(connData, "\n", -1);
        }

        if (caps.colorsys.start != caps.colorsys.end && !strncmp(caps.colorsys.start, "rgbw", 4)) {
            msg = "Unsupported color space\n";
            goto out;
        }

        if (!caps.v3.start) {
            msg = "Not enough channels sent\n";
            goto out;
        }

        if (caps.fade_cont.start) {
            msg = "Fade not yet supported\n";
            goto out;
        }

        httpdSend(connData, "Parsing numbers\n", -1);
        int v0 = atoi(caps.v0.start),
            v1 = atoi(caps.v1.start),
            v2 = atoi(caps.v2.start),
            v3 = atoi(caps.v3.start+1);

        if (v0 > 0xffff || v1 > 0xffff || v2 > 0xffff || v2 > 0xffff) {
            msg = "Channel value too large\n";
            goto out;
        }

        httpdSend(connData, "Setting channel values\n", -1);
        /* Note that this will clobber the framebuf if there is an error in the second channel. But that's ok. */
        currentFramebuf.chs[i] = (struct channel) {
            .r = v0, .g=v1, .b=v2, .w=v3
        };
    }

    httpdSend(connData, "Sending data\n", -1);
    send_framebuf(&currentFramebuf);

out:
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
