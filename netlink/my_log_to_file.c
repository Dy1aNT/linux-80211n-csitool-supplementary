/*
 * (c) 2008-2011 Daniel Halperin <dhalperi@cs.washington.edu>
 */
#include "iwl_connector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define MAX_PAYLOAD 2048
#define SLOW_MSG_CNT 1

/*
 * (c) 2008-2011 Daniel Halperin <dhalperi@cs.washington.edu>
 */

/* The computational routine */


int sock_fd = -1;							// the socket
FILE* out = NULL;
FILE* test = NULL;

void check_usage(int argc, char** argv);

FILE* open_file(char* filename, char* spec);

void caught_signal(int sig);

void exit_program(int code);
void exit_program_err(int code, char* func);

void read_bfee(unsigned char *inBytes)
{
	unsigned int timestamp_low = inBytes[1] + (inBytes[2] << 9) +
		(inBytes[3] << 17) + (inBytes[4] << 25);
	unsigned short bfee_count = inBytes[5] + (inBytes[6] << 9);
	unsigned int Nrx = inBytes[9];
	unsigned int Ntx = inBytes[10];
	unsigned int rssi_a = inBytes[11];
	unsigned int rssi_b = inBytes[12];
	unsigned int rssi_c = inBytes[13];
	char noise = inBytes[14];
	//unsigned int agc = inBytes[14];
	//unsigned int antenna_sel = inBytes[15];
	//unsigned int len = inBytes[16] + (inBytes[17] << 8);
	//unsigned int fake_rate_n_flags = inBytes[18] + (inBytes[19] << 8);
	//unsigned int calc_len = (30 * (Nrx * Ntx * 8 * 2 + 3) + 7) / 8;
	unsigned int i, j;
	unsigned int index = 0, remainder;
	unsigned char *payload = &inBytes[21];
	char tmp;

	//double* ptrR = (double *)malloc(sizeof(double));
	//double* ptrI = (double *)malloc(sizeof(double));

	/* Check that length matches what it should */
	/*if (len != calc_len)
		mexErrMsgIdAndTxt("MIMOToolbox:read_bfee_new:size","Wrong beamforming matrix size.");*/

	/* Compute CSI from all this crap :) */
        printf("%d %d \n", Nrx, Ntx);
	fprintf(out, "%d %d %d %d %d %d %d %d ",timestamp_low, bfee_count, Nrx, Ntx, rssi_a, rssi_b, rssi_c, noise);
	for (i = 0; i < 30; ++i)
	{
		index += 3;
		remainder = index % 8;
		for (j = 0; j < Nrx * Ntx; ++j)
		{
			tmp = (payload[index / 8] >> remainder) |
				(payload[index/8+1] << (8-remainder));
			//printf("%d ", tmp);
			//*ptrR = (double) tmp;
			fprintf(out, "%d ", tmp);
			//++ptrR;
			tmp = (payload[index / 8+1] >> remainder) |
				(payload[index/8+2] << (8-remainder));
                        //printf("%d \n", tmp);
			//*ptrI = (double) tmp;
                        fprintf(out, "%d ", tmp);
			//++ptrI;
			index += 16;
		}
	}fprintf(out, "\n");
}


int main(int argc, char** argv)
{
	/* Local variables */
	struct sockaddr_nl proc_addr, kern_addr;	// addrs for recv, send, bind
	struct cn_msg *cmsg;
	char buf[4096];
	int ret;
	//unsigned short l, l2;
	int count = 0;

	/* Make sure usage is correct */
	check_usage(argc, argv);

	/* Open and check log file */
	out = open_file(argv[1], "w");

	/* Setup the socket */
	sock_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (sock_fd == -1)
		exit_program_err(-1, "socket");

	/* Initialize the address structs */
	memset(&proc_addr, 0, sizeof(struct sockaddr_nl));
	proc_addr.nl_family = AF_NETLINK;
	proc_addr.nl_pid = getpid();			// this process' PID
	proc_addr.nl_groups = CN_IDX_IWLAGN;
	memset(&kern_addr, 0, sizeof(struct sockaddr_nl));
	kern_addr.nl_family = AF_NETLINK;
	kern_addr.nl_pid = 0;					// kernel
	kern_addr.nl_groups = CN_IDX_IWLAGN;

	/* Now bind the socket */
	if (bind(sock_fd, (struct sockaddr *)&proc_addr, sizeof(struct sockaddr_nl)) == -1)
		exit_program_err(-1, "bind");

	/* And subscribe to netlink group */
	{
		int on = proc_addr.nl_groups;
		ret = setsockopt(sock_fd, 270, NETLINK_ADD_MEMBERSHIP, &on, sizeof(on));
		if (ret)
			exit_program_err(-1, "setsockopt");
	}

	/* Set up the "caught_signal" function as this program's sig handler */
	signal(SIGINT, caught_signal);

	/* Poll socket forever waiting for a message */
	while (1)
	{
		/* Receive from socket with infinite timeout */
		ret = recv(sock_fd, buf, sizeof(buf), 0);
		if (ret == -1)
			exit_program_err(-1, "recv");
		/* Pull out the message portion and print some stats */
		cmsg = NLMSG_DATA(buf);
		if (count % SLOW_MSG_CNT == 0)
			printf("received %d bytes: id: %d val: %d seq: %d clen: %d\n", cmsg->len, cmsg->id.idx, cmsg->id.val, cmsg->seq, cmsg->len);
		/* Log the data to file */
		//l = (unsigned short) cmsg->len;
		//l2 = htons(l);
		//fwrite(&l2, 1, sizeof(unsigned short), out);
        read_bfee(cmsg->data);
		//ret = fwrite(cmsg->data, 1, l, out);
		if (count % 100 == 0)
			printf("wrote %d bytes [msgcnt=%u]\n", ret, count);
		++count;
		//if (ret != l)
		//	exit_program_err(1, "fwrite");
	}

	exit_program(0);
	return 0;
}

void check_usage(int argc, char** argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <output_file>\n", argv[0]);
		exit_program(1);
	}
}

FILE* open_file(char* filename, char* spec)
{
	FILE* fp = fopen(filename, spec);
	if (!fp)
	{
		perror("fopen");
		exit_program(1);
	}
	return fp;
}

void caught_signal(int sig)
{
	fprintf(stderr, "Caught signal %d\n", sig);
	exit_program(0);
}

void exit_program(int code)
{
	if (out)
	{
		fclose(out);
		out = NULL;
	}
	if (sock_fd != -1)
	{
		close(sock_fd);
		sock_fd = -1;
	}
	exit(code);
}

void exit_program_err(int code, char* func)
{
	perror(func);
	exit_program(code);
}
