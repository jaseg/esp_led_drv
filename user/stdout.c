//Stupid bit of code that does the bare minimum to make os_printf work.

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <esp8266.h>
#include <uart_hw.h>

static void ICACHE_FLASH_ATTR mutePutchar(char c) {
    /* do nothing */
}


void stdoutInit() {
	//Install our own putchar handler
	os_install_putc1((void *)mutePutchar);
}
