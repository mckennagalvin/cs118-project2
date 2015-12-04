/*
 packet.c
 packets consist of header information and data
 maximum packet size is 1KB
 */

#define PACKET_SIZE 100
#define RETRANSMIT_TIMEOUT 500
#define TIME_WAIT 1520

#define TYPE_DATA 0
#define TYPE_FINAL_DATA 1
#define TYPE_REQUEST 2
#define TYPE_ACK 3

#define RESULT_PACKET_LOSS 0
#define RESULT_PACKET_CORRUPT 1
#define RESULT_PACKET_OUT_OF_ORDER 2
#define RESULT_PACKET_OK 3

struct packet {
 	int seq;
 	int type;
 	int length;
 	char data[PACKET_SIZE];
 	uint16_t checksum;
};


// computes checksum: 1's complement of all 16-bit words in the packet
// reference: http://minirighi.sourceforge.net/html/udp_8c.html

uint16_t compute_checksum(const uint16_t * buf, size_t len) {

	// calculate the sum
	uint32_t sum = 0;
	while (len > 1) {
		sum += *buf++;
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	// add padding if packet length is odd
	if (len & 1)
		sum += *((uint8_t *)buf);

	// add the carries
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	// checksum is 1's complement of sum
	return (uint16_t)(~sum);
}

void checksum(struct packet * p) {
	size_t len = (size_t) p->length + (3 * sizeof(int));
	p->checksum = compute_checksum((const uint16_t *) p, len);
}

int corrupt(struct packet * p) {
	size_t len = (size_t) p->length + (3 * sizeof(int));
    uint16_t checksum_before = p->checksum;
    uint16_t checksum_after  = compute_checksum((const uint16_t *) p, len);
    // DEBUGGING
    //printf("checksum before was %u, now it's %u\n", checksum_before, checksum_after);
    return checksum_before != checksum_after;
}

int rdt_receive(int sockfd, void * p, size_t len, struct sockaddr *src_addr, socklen_t *addrlen, double lossprob, double corruptprob, int expected_seq) {

	struct packet * pkt = (struct packet *)p;

	// receive packet - check for actual packet loss
	if (recvfrom(sockfd, p, len, 0, src_addr, addrlen) < 0) {
		printf("Packet was not received\n");
		return RESULT_PACKET_LOSS;
	}

	// lose packet according to packet loss probability
  	double random = (double)rand()/(double)RAND_MAX;
  	if (random < lossprob) {
  		printf("Packet with seq#%d was simulated to be lost.\n", pkt->seq);
		return RESULT_PACKET_LOSS;
  	}

	// corrupt data according to corrupt packet probability
	random = (double)rand()/(double)RAND_MAX;
  	if (random < corruptprob) {
  		printf("Packet with seq#%d was simulated to be corrupt.\n", pkt->seq);
		return RESULT_PACKET_CORRUPT;
  	}

  	// check for actual corruption
  	if (corrupt(pkt)) {
  		printf("Packet with seq#%d was found to be corrupt.\n", pkt->seq);
  		return RESULT_PACKET_CORRUPT;
  	}

  	// check that packet is in correct order
  	if (pkt->seq != expected_seq) {
  		printf("Packet out of order: expected seq#%d, received seq#%d. Packet discarded.\n", expected_seq, pkt->seq);
  		return RESULT_PACKET_OUT_OF_ORDER;
  	}

  	// if it gets to this point, packet has arrived successfully in order and is not corrupt
  	printf("Received seq#%d successfully. Packet is not corrupt.\n", pkt->seq);
  	return RESULT_PACKET_OK;

}


