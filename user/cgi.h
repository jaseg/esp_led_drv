#ifndef CGI_H
#define CGI_H

#include "httpd.h"

int cgiRgbw(HttpdConnData *connData);
int cgiLed(HttpdConnData *connData);
int tplLed(HttpdConnData *connData, char *token, void **arg);
int tplCounter(HttpdConnData *connData, char *token, void **arg);

#endif
