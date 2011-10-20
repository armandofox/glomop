#include "communication.h"

int main(int argc, char **argv)
{
  MulticastSocket sock;
  char buffer[500], ipAddress[80];
  int len, port=0, ttl=0;

  strcpy(ipAddress, "");
  if (argc!=2) {
    fprintf(stderr, "Usage: %s multicast-addr/multicast-port[/ttl]\n",argv[0]);
    return -1;
  }

  sscanf(argv[1], "%[^/]/%d/%d", ipAddress, &port, &ttl);
  if (sock.CreateSender(ipAddress, port, ttl)==FALSE) {
    printf("Couldn't create multicast socket\n");
    return 1;
  }

  while(1) {
    printf("--> ");
    gets(buffer);
    if (strcmp(buffer, "quit")==0 || strcmp(buffer, "exit")==0) break;

    len = strlen(buffer);
    while(len > 0 && (buffer[len-1]=='\n' || buffer[len-1]=='\r')) 
      buffer[--len] = '\0';

    int read=0, write = 0;
    while(read <= len) {
      if (buffer[read]=='\\' && buffer[read+1]=='n') { 
	buffer[write] = '\n';
	read += 2;
      }
      else {
	if (buffer[read]==';') {
	  buffer[write] = '\001';
	}
	else {
	  buffer[write] = buffer[read];
	}
	read++;
      }
      write++;
    }

    if (sock.Write(buffer, strlen(buffer)+1)==FALSE) {
      printf("Write error\n");
    }
  }

  return 0;
}

