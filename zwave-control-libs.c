/*
 ============================================================================
 Name        : zwave-control-libs.c
 Author      : cj
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void dao_zdevices_setattrib(int node, const char name, const char *format, ...) {

}

void zw_hook_network_addnodestatus(int status, uint8_t node) //入网状态反馈
{

}

void zw_hook_network_removenodestatus(int status, uint8_t node) {

} //状态反馈

void zw_hook_network_removefailednode(int status, uint8_t node) {

}
void zw_hook_network_setdefault() {

}

void zwave_hook_apphandler(unsigned char node, const void *data, int dlen) {

}

void server(void *context) {

}

int main(void) {
	hal_init("/dev/ttyACM0");
	sleep(1);
	zw_dispatch_async(server, NULL);
	sleep(100);
	return EXIT_SUCCESS;
}
