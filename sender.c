/*
 sender.c
 server which services the file requested by the client
 usage: ./sender <port number>
 */

#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>    // for the waitpid() system call
#include <signal.h>	     // signal name macros, and the kill() prototype

#include "packet.c"

void error(char *msg)
{
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, recvlen;
  struct sockaddr_in serv_addr, cli_addr;
  socklen_t clilen = sizeof(cli_addr);
  struct packet send;
  struct packet receive;
  FILE * f;
  char * filename;
  int filesize;
  int numpackets;
  int i;

  if (argc < 2) {
    fprintf(stderr,"ERROR, no port provided\n");
    exit(1);
  }

  // create UDP socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  // fill in address info
  memset((char *) &serv_addr, 0, sizeof(serv_addr));  //reset memory
  portno = atoi(argv[1]);
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
    printf("received %d bytes\n", receive.length);

    // debugging
    printf("request type: %d\n", receive.type);
    printf("file requested: %s\n", receive.data);

    // open file
    if (receive.type == TYPE_REQUEST) {
      f = fopen(receive.data, "r");
      if (f == NULL)
        error("ERROR opening file");
    }

    // find file size and determine number of packets needed
    fseek(f, 0, SEEK_END);
    filesize = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("requested file is %d bytes", filesize);
    numpackets = filesize / PACKET_SIZE;
    if (filesize % PACKET_SIZE > 0)
      numpackets++;
    printf("%d packets are needed to send the file", numpackets);

    // split data into packets and send
    for (i = 0; i < numpackets; i++) {
      memset((char *) &send, 0, sizeof(send));
      send.seq = i + 1;
      send.length = fread(send.data, sizeof(char), PACKET_SIZE, f);
      send.type = TYPE_DATA;
      if (sendto(sockfd, &send, sizeof(send), 0, (struct sockaddr *) &cli_addr, clilen) < 0)
        error("ERROR sending data to client");
      printf("sent packet %d to client", send.seq);
    }

  
  }
     
  // never reached if we never break out of the loop but whatever
  close(sockfd); 
  return 0; 
}

