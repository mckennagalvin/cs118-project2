/*
 packet.c
 packets consist of header information and data
 maximum packet size is 1KB
 */

#define PACKET_SIZE 100
#define TYPE_DATA 0
#define TYPE_FINAL_DATA 1
#define TYPE_REQUEST 2
#define TYPE_ACK 3

struct packet {
	char data[PACKET_SIZE];
 	int seq;
 	int type;
 	int length;
 	uint16_t checksum;
};


// computes checksum: 1's complement of all 16-bit words in the packet
// reference: http://minirighi.sourceforge.net/html/udp_8c.html

void compute_checksum(struct packet * p) {
	const uint16_t * buf = (const void *) p->data;
	size_t len = p->length;

	// calculate the sum
	uint32_t sum = 0;
	while (len > 1) {
		sum += *buf++;
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	// add padding if packet lenght is odd
	if (len & 1)
		sum += *((uint8_t *)buf);

	// add the carries
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	// checksum is 1's complement of sum
	p->checksum = (uint16_t)(~sum);
}

