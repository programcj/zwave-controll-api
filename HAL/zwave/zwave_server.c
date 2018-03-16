/*
 * zwave_server.c
 *
 *  Created on: 2017年12月12日
 *      Author: cj
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include "zwave_server.h"

#include "config.h"

static volatile int client_count = 0;
static struct list_head _listclients;
static struct list_queue _queue_netmsg;

struct net_msg_t {
	struct list_head list;
	unsigned int ID;
	int dlen;
	unsigned char data[0xFF];
};

struct packet_t {
	int len;
	uint8_t data[0xFF];  //数据最大只有255个
	int _rpos;
	int _rlen;
};

struct client_t {
	struct list_head list;
	unsigned int ID;
	int sock;
	int pollfd_index;
	struct packet_t in_packet;
	int time_last;
};

void _client_remove(struct client_t *client) {
	list_del(&client->list);
	client_count--;
}

void _client_add(struct client_t *client) {
	list_add(&client->list, &_listclients);
	client_count++;
}

int socket_setnonblock(int sock) {
	int opt;
	/* Set non-blocking */
	opt = fcntl(sock, F_GETFL, 0);
	if (opt == -1) {
		close(sock);
		return 1;
	}
	if (fcntl(sock, F_SETFL, opt | O_NONBLOCK) == -1) {
		/* If either fcntl fails, don't want to allow this client to connect. */
		close(sock);
		return 1;
	}
	return 0;
}

int zclient_close(struct client_t *client) {
	shutdown(client->sock, SHUT_RDWR);
	close(client->sock);
	client->sock = -1;
	_client_remove(client);
	return 0;
}

int zclient_accept(int sockfd, unsigned int ID) {
	int sock = accept(sockfd, NULL, NULL);
	if (sock == -1)
		return -1;
	if (socket_setnonblock(sock)) {
		return -1;
	}
	struct client_t *client = (struct client_t*) malloc(
			sizeof(struct client_t));
	memset(client, 0, sizeof(struct client_t));
	client->sock = sock;
	client->ID = ID;
	client->time_last = time(NULL);
	_client_add(client);
	return sock;
}

int zclient_read(struct client_t *client) {
	uint8_t bytes;
	int rlen = 0;

	if (client->in_packet.len == 0) {
		rlen = read(client->sock, &bytes, 1);
		if (rlen == 1) {
			client->in_packet.len = rlen;
			client->in_packet._rlen = rlen;
			client->in_packet._rpos = 0;
			memset(client->in_packet.data, 0, sizeof(client->in_packet.data));
		} else if (rlen == 0)
			return -1;
		else {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;
			return -1;
		}
	}
	while (client->in_packet._rlen) {
		rlen = read(client->sock,
				client->in_packet.data + client->in_packet._rpos,
				client->in_packet._rlen);
		if (rlen > 0) {
			client->in_packet._rlen -= rlen;
			client->in_packet._rpos += rlen;
		} else if (rlen == 0)
			return -1;
		else {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;
			return -1;
		}
	}

	//handle packet
	{
		struct net_msg_t *msg = (struct net_msg_t*) malloc(
				sizeof(struct net_msg_t));
		msg->ID = client->ID;
		msg->dlen = client->in_packet.len;
		memcpy(msg->data, client->in_packet.data, client->in_packet.len);
		list_queue_append_tail2(&_queue_netmsg, &msg->list, 0);
	}
	client->in_packet.len = 0;
	client->time_last = time(NULL);
}

void loop_handle_rw(struct pollfd *pollfds) {
	struct list_head *p, *n;
	struct client_t *client;

	list_for_each_safe(p,n,&_listclients)
	{
		client = list_entry(p, struct client_t, list);
		if (client->pollfd_index < 0) {
			continue;
		}
		if (pollfds[client->pollfd_index].revents & POLLOUT) {

		}
	}
	//////////////////////////////////////////////////////////////
	list_for_each_safe(p,n,&_listclients)
	{
		client = list_entry(p, struct client_t, list);
		if (client->pollfd_index < 0) {
			continue;
		}
		if (pollfds[client->pollfd_index].revents & POLLIN) {
			if (zclient_read(client)) {
				zclient_close(client);
			}
		}
	}
}

void zwave_server_loop(int sockfd) {
	struct pollfd *pollfds = NULL;
	int pollfd_count;
	int pollfd_index = 0;
	int fdcount = 0;
	int i = 0;
	struct list_head *p, *n;
	struct client_t *client;
	unsigned int ID = 0;
	int now_time = 0;

	sigset_t sigblock, origsig;
	sigemptyset(&sigblock);
	sigaddset(&sigblock, SIGINT);

	while (1) {
		if (!pollfds || client_count + 1 > pollfd_count) {
			pollfd_count = client_count + 1;
			pollfds = realloc(pollfds, pollfd_count * sizeof(struct pollfd));
		}
		memset(pollfds, -1, pollfd_count * sizeof(struct pollfd));

		pollfds[0].fd = sockfd;
		pollfds[0].events = POLLIN;
		pollfds[0].revents = 0;
		pollfd_index = 1;

		now_time = time(NULL);

		list_for_each_safe(p,n,&_listclients)
		{
			client = list_entry(p, struct client_t, list);
			pollfds[pollfd_index].fd = client->sock;
			pollfds[pollfd_index].events = POLLIN;
			pollfds[pollfd_index].revents = 0;
			//if(context->current_out_packet || context->state == mosq_cs_connect_pending){
			//	pollfds[pollfd_index].events |= POLLOUT;
			//}
			client->pollfd_index = pollfd_index;
			pollfd_index++;
		}
		sigprocmask(SIG_SETMASK, &sigblock, &origsig);
		fdcount = poll(pollfds, pollfd_index, 100);
		sigprocmask(SIG_SETMASK, &origsig, NULL);
		if (fdcount == -1) {

		} else {
			loop_handle_rw(pollfds);

			if (pollfds[0].revents & (POLLIN | POLLPRI)) {
				zclient_accept(sockfd, ID++);
			}
		}
	}

}

void *_t_zserver(void *args) {
	struct sockaddr_un address;
	int sockfd;
	int len;
	int i, bytes;
	int result;
	char ch_recv, ch_send;
	pthread_detach(pthread_self());

	unlink("server_socket");

	/*创建socket,AF_UNIX通信协议,SOCK_STREAM数据方式*/
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, "server_socket");
	len = sizeof(address);

	bind(sockfd, (struct sockaddr *) &address, len);
	/*监听网络,队列数为5*/
	listen(sockfd, 5);

	zwave_server_loop(sockfd);
	return NULL;
}

void zwave_server_init() {
	pthread_t pt;
	INIT_LIST_HEAD(&_listclients);
	list_queue_init(&_queue_netmsg);
	pthread_create(&pt, NULL, _t_zserver, NULL);
}

void zwave_server_handle() {
	struct list_head *p;
	struct net_msg_t *msg = NULL;
	p = list_queue_pop(&_queue_netmsg);
	if (!p)
		return;
	msg = list_entry(p, struct net_msg_t, list);

	free(msg);
}
