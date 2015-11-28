/*
 packet.c
 packets consist of header information and data
 maximum packet size is 1KB
 */

 #define PACKET_SIZE 100
 #define TYPE_DATA 0
 #define TYPE_REQUEST 1
 #define TYPE_ACK 2

 struct packet {
 	char data[PACKET_SIZE];
 	int seq;
 	int type;
 	int length;
 };