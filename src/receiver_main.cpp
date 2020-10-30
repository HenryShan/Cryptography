/* 
 * File:   receiver_main.cpp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <unordered_map>
#include <time.h>
#include <sys/time.h>
#include <iostream>
#include <cerrno> 
#include <cstring>
#include <thread>
#include <mutex>
#include "dataStructure.h"

using namespace std;



struct sockaddr_in si_me, si_other;
int s, slen;
FILE *fp = NULL;
unsigned long long total_loaded = 0;

int flag = 0;
short last_seq = -1;
std::unordered_map<int, Packet*> recv_buf;
std::mutex mtx;

void diep(string s) {
    perror(s.c_str());
    exit(1);
}

void loadtoFile_mul(int start, int end) {
    if (fp != NULL) {
		for (int i = start; i <= end; i++) {
			Packet *pkt = recv_buf[i];
			mtx.lock();
			int loaded = fwrite(pkt->data, sizeof(char), pkt->pktsize, fp);
        	if (loaded != pkt->pktsize || loaded != 1024) {
            	fprintf(stderr, "Error while write sequence num %d", i);
        	}
        	total_loaded += loaded;
			mtx.unlock();
		}
		return;
    } else {
        fprintf(stderr, "fp is not open!\n");
		exit(1);
        return;
    }
}




void loadtoFile(int i) {
    if (fp != NULL) {
		Packet *pkt = recv_buf[i];
		int loaded = fwrite(pkt->data, sizeof(char), pkt->pktsize, fp);
    	if (loaded < pkt->pktsize ) {
    		fprintf(stderr, "a PKT output less bytes than it stored %d", i);
    	}
    	total_loaded += loaded;
    	return;
    } else {
        fprintf(stderr, "fp is not open!\n");
		exit(1);
        return;
    }
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    remove(destinationFile);
    fp = fopen(destinationFile, "ab");
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket creation failed!\n");

	// add timeout function
	struct timeval receiver_timeout; // now the time interval for timeout is 35 seconds.
	receiver_timeout.tv_sec = 35;
	receiver_timeout.tv_usec = 0; //microseconds
	if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiver_timeout, sizeof(receiver_timeout))== -1) {
		fprintf(stderr, "failed to set socket opt.!\n");
		exit(1);
	}
    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("failed to bind address!\n");

    int max_seq = 0;
	int next_seq = 0;
	int reply_ack_seq_num = 0;
    bool finished = false;

    while (!finished) {
        Packet *pkt = new Packet();
        socklen_t temp = slen;
        int recv_bytes = recvfrom(s, pkt, sizeof(*pkt), 0, (struct sockaddr *)&si_other, &temp);

        if (recv_bytes == -1) {
	    // add timeout check
	    	if (errno == ETIMEDOUT || errno == EAGAIN || errno == EWOULDBLOCK) {
				fprintf(stderr, "receiving the file timeout!\n");
				exit(1);			
			} else {
				fprintf(stderr, "Failed to receive the file: %s\n", strerror(errno));
				exit(1);
			}
        }
        if (pkt->seq_num == -1) {
			if (flag) {
				continue;			
			}
            last_seq = pkt->pktsize;
			if ((int) last_seq >= next_seq) {
            	flag = 1;
			} else {
				finished = true;
			}
			continue;
        } else {
			
            max_seq = max(max_seq, pkt->seq_num);
            if (recv_buf.find(pkt->seq_num) == recv_buf.end()) {
                recv_buf[pkt->seq_num] = pkt;
            }
        }

	//printf("Recv seq num is %d!\n", pkt->seq_num);
        if (pkt->seq_num == next_seq) {
            	while (recv_buf.find(next_seq) != recv_buf.end()) {
                	loadtoFile(next_seq);
                	next_seq++;
            	}
            	reply_ack_seq_num = next_seq - 1;
        }
		if (flag) {
			if (reply_ack_seq_num == (int) last_seq) {
				finished = true;
				AckMessage *ack_message = new AckMessage();
				ack_message->ack_num = reply_ack_seq_num;	
				for (int i = 0; i < 5; i++) {
					if (sendto(s, ack_message, sizeof(*ack_message), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1) {
            			diep("Error ACK replying!");
       		 		}
				}
				break;
			}
		}
        AckMessage *ack_message = new AckMessage();
		ack_message->ack_num = reply_ack_seq_num;

        if (sendto(s, ack_message, sizeof(*ack_message), 0, (struct sockaddr *)&si_other, sizeof(si_other)) == -1) {
            diep("Error ACK replying!");
        }
        //fprintf(stderr, "	max_seq now: %d\n", max_seq);
    }
    if (fclose(fp) != 0) {
        diep("File write terminate fail!\n");
    }
	//fprintf(stderr, "total wroten bytes = %llu and next_seq = %d\n", total_loaded, next_seq);
    close(s);
    return;
}



/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);
	slen = sizeof (si_other);
    reliablyReceive(udpPort, argv[2]);
}
