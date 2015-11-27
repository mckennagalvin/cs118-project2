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


void error(char *msg)
{
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[])
{
  int sockfd, newsockfd, portno, recvlen;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  char recv_buf[2048]; // receive buffer
  char * send_message = "this is a test, sender to receiver";



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

    // receieve data from client
    recvlen = recvfrom(sockfd, recv_buf, 2048, 0, (struct sockaddr *) &cli_addr, &clilen);
    if (recvlen < 0)
      error("ERROR receiving data from client");
    printf("received %d bytes\n", recvlen);

    // send response back
    if (sendto(sockfd, send_message, strlen(send_message) + 1, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
      error("ERROR sending data to client");
    printf("sent to client");
  }
     
  // never reached if we never break out of the loop but whatever
  close(sockfd); 
  return 0; 
}

