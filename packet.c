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
 };