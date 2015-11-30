/*
 sender.c
 server which services the file requested by the client
 usage: ./sender <port number> <CWnd size> <loss probability> <corruption probability>
 */

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>    // for the waitpid() system call
#include <signal.h>	     // signal name macros, and the kill() prototype
#include <time.h>

#include "packet.c"

void error(char *msg)
{
  perror(msg);
  exit(1);
}

// Listens for an ack, returning the ack number
int listen_for_ack(int sockfd) {
  int recvlen;
  int ack_number = -1;
  struct packet receive;

  while (ack_number == -1) {
    recvlen = recvfrom(sockfd, &receive, sizeof(receive), 0, NULL, NULL);
    if (recvlen < 0)
      error("ERROR receiving ack from client");
    if (receive.type != TYPE_ACK) {
      fprintf(stderr, "Expected ACK, received type %d", receive.type);
      continue;
    }
    ack_number = receive.seq;
  } 
  printf("Received ack %d.\n", ack_number);
  return ack_number;
}

// Make data corrupt according to probability passed in as parameter
// (this is done after the checksum is computed, so the checksum at the receiver's end will not match the checksum in the packet)
void corrupt_packet(struct packet * p, double corruptprob) {
  double random = (double)rand()/(double)RAND_MAX;
  if (random < corruptprob) {
    // corrupt packet (currently just changes the first character in data, TODO make more robust??)
    char c = p->data[0];
    p->data[0] = (char)(c + 1);
  }
}

// Sends an individual packet over UDP
void send_packet(struct packet pkt, int sockfd, struct sockaddr_in cli_addr, socklen_t clilen, double corruptprob) 
{
  int type = pkt.type;
  char readable_type[11];
  if (type == TYPE_DATA)
    strcpy(readable_type, "data");
  else if (type == TYPE_FINAL_DATA)
    strcpy(readable_type, "final_data");
  else
    strcpy(readable_type, "non-data");

  // corrupt data according to parameter
  corrupt_packet(&pkt, corruptprob);

  if (sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *) &cli_addr, clilen) < 0)
    error("ERROR sending data to client");
  printf("Sent %s packet %d to client.\n", readable_type, pkt.seq);

  // DEBUGGING
  printf("checksum: %u\n", pkt.checksum);
}

// Reliably sends an array of packets
void rdt_send_packets(struct packet *packets, int sockfd, struct sockaddr_in cli_addr, socklen_t clilen, int cwndsize, double corruptprob)
{
  int nextseqnum = 0;
  int base = 0;
  int all_sent = 0;

  while(!all_sent || (base < nextseqnum)) {
    while (nextseqnum < base + cwndsize && !all_sent) {
      send_packet(packets[nextseqnum], sockfd, cli_addr, clilen, corruptprob);
      if (packets[nextseqnum].type == TYPE_FINAL_DATA) {
        all_sent = 1;
        nextseqnum++;
        break;
      }
      nextseqnum++;
    }
    base = listen_for_ack(sockfd) + 1;
  }
}

// Returns an array of packets needed to send file f
struct packet * prepare_packets(FILE * f) 
{
  int i;
  int filesize;
  int numpackets;
  struct packet send;
  struct packet *packets;

  // find file size and determine number of packets needed
  fseek(f, 0, SEEK_END);
  filesize = ftell(f);
  fseek(f, 0, SEEK_SET);
  printf("Requested file is %d bytes. ", filesize);
  numpackets = filesize / PACKET_SIZE;
  if (filesize % PACKET_SIZE > 0)
    numpackets++;
  printf("%d packets are needed to send the file.\n", numpackets);

  packets = (struct packet*)malloc(sizeof(struct packet) * numpackets);
  
  // split data into packets and send
  for (i = 0; i < numpackets; i++) {
    memset((char *) &send, 0, sizeof(send));
    send.seq = i;
    send.length = fread(send.data, sizeof(char), PACKET_SIZE, f);
    
    if (i == numpackets-1) 
      send.type = TYPE_FINAL_DATA;
    else 
      send.type = TYPE_DATA;
    
    checksum(&send);

    packets[i] = send;
    // send_packet(send, sockfd, cli_addr, clilen);
  }
  return packets;
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, recvlen;
  struct sockaddr_in serv_addr, cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  struct packet receive;
  FILE * f;
  char * filename;
  int i;
  double lossprob, corruptprob;
  int cwndsize;
  struct packet *packets;

  // initialize random number generator
  time_t t;
  srand((unsigned) time(&t));

  if (argc == 2) {
    portno = atoi(argv[1]);
    cwndsize = 1;
    lossprob = 0;
    corruptprob = 0;
  } else if (argc < 5) {
    fprintf(stderr,"usage: %s <port number> <CWnd size> <loss probability> <corruption probability>\n", argv[0]);
    exit(1);
  } else {
    portno = atoi(argv[1]);
    cwndsize = atoi(argv[2]);
    lossprob = atof(argv[3]);
    corruptprob = atof(argv[4]);
  }

  printf("CWnd size: %d\nProbLoss: %f\nProbCorrupt: %f\nServer listing on port %d...\n\n", cwndsize, lossprob, corruptprob, portno);

  // create UDP socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  // fill in address info
  memset((char *) &serv_addr, 0, sizeof(serv_addr));  //reset memory
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  // bind socket to IP address and port number
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
    error("ERROR on binding");
  
  // scan for requests from client
  while (1) {

    // receieve request from client
    recvlen = recvfrom(sockfd, &receive, sizeof(receive), 0, (struct sockaddr *) &cli_addr, &clilen);
    if (recvlen < 0)
      error("ERROR receiving data from client");
    printf("Received request (%d bytes) for %s\n", receive.length, receive.data);

    // open file
    if (receive.type == TYPE_REQUEST) {
      f = fopen(receive.data, "r");
      if (f == NULL)
        error("ERROR opening file");
    } else {
      fprintf(stderr, "Packet not of type request\n");
      continue;
    }

    packets = prepare_packets(f);
    rdt_send_packets(packets, sockfd, cli_addr, clilen, cwndsize, corruptprob);
    free(packets);
    
    printf("Finished sending file. Listening for new request...\n\n");
  }
     
  // never reached if we never break out of the loop but whatever
  close(sockfd); 
  return 0; 
}



