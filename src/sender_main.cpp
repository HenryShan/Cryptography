/* 
 * File:   sender_main.cpp
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <unordered_map>
#include <queue>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <cmath>
#include <semaphore.h>
#include "dataStructure.h"

using namespace std;

struct sockaddr_in si_other;
int s, slen;

#define ALPHA 0.125
#define BETA 0.25
// Three states of sender.
enum SenderStatus {slow_start, congestion_avoidance, fast_recovery};

struct Sender{
	bool complete;
    struct timeval timeout;
    int dup_ack_count;
	int CWBase;
    int curr_seq; 
    SenderStatus status;
	int SSThreshold;
    double CW;
};

auto start_time = std::chrono::high_resolution_clock::now();

static Sender sender;
static int max_ACK;
static std::unordered_map<int, Packet*> send_buf;
static std::queue<PacketSendTime> send_seq;
static struct timeval srtt;
static struct timeval rttval;
static struct timeval rto;

int tot_timeout;
int total_packets = 0;

void diep(char *s) {
    perror(s);
    exit(1);
}



// when received new ACK, calculate new RTO accordingly.
void calcu_new_rto(int receive_seq, struct timeval time_now) {
	while(send_seq.size() > 0 && send_seq.front().seq_num < receive_seq) {
		send_seq.pop();	
	}
	if (send_seq.size() == 0) {
		return;
	}
	//fprintf(stderr, "front's seq_num = %d\n", send_seq.front().seq_num);
	auto total_microsec_of_currRTT = (time_now.tv_sec - send_seq.front().tv.tv_sec) * 1000000 + (time_now.tv_usec - send_seq.front().tv.tv_usec);
	//fprintf(stderr, "micro secs of newRTT = %d\n", (int) total_microsec_of_currRTT);
	auto total_microsec_of_srtt = srtt.tv_sec * 1000000 + srtt.tv_usec;
	auto total_microsec_of_rttval = rttval.tv_sec * 1000000 + rttval.tv_usec;
	auto final_srtt_micros = ((1.0-ALPHA)* total_microsec_of_srtt + ALPHA * total_microsec_of_currRTT);
	auto final_rttval_micros = ((1.0-BETA)* total_microsec_of_rttval + BETA * abs(total_microsec_of_currRTT - total_microsec_of_rttval));
	//fprintf(stderr, "fin_srtt = %d usec\n", (int) final_srtt_micros);
	//fprintf(stderr, "fin_rttval = %d usec\n", (int) final_rttval_micros);
	srtt.tv_sec = final_srtt_micros / 1000000;
	srtt.tv_usec = (int) final_srtt_micros % 1000000;
	rttval.tv_sec = final_rttval_micros / 1000000;
	rttval.tv_usec = (int) final_rttval_micros % 1000000;
	auto rto_tot_ms = final_srtt_micros + 4 * final_rttval_micros;
	//fprintf(stderr, "============================new rto tot = %d usec\n", (int) rto_tot_ms);
	rto.tv_sec = rto_tot_ms / 1000000;
	rto.tv_usec = (int) rto_tot_ms % 1000000;
}

// give sequence number and send corresponding packet. 
void send_PKT(int seq_num) {

	//printf("Sending %d packet\n", seq_num);

    Packet* pkt = send_buf[seq_num];
    struct sockaddr* to_addr = (struct sockaddr *)&si_other;
    socklen_t temp = slen;
    if (sendto(s, pkt, sizeof(*pkt), 0, to_addr, temp) == -1) {
        diep("Error while sending packet!\n");
    }
}

void readFileToBuffer(char* filename, unsigned long long int bytesToTransfer){
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file to send.\n");
        exit(1);
    }

	/* Determine how many bytes to transfer */
    // first, load file into PKTs and store them in map
    unsigned long long int rest_num_of_bytes = bytesToTransfer;
    int seq = 0;
    while (!feof(fp) && rest_num_of_bytes > 0) {
        // Not sure if we need to allocate the PKT on heap or just stack is fine...?
        Packet *new_PKT = new Packet;
		memset((char*)new_PKT, 0, sizeof(*new_PKT));
        //new_PKT.type = DATA;
        new_PKT->seq_num = seq;
		int bytesRead = 0;
       	if (rest_num_of_bytes < PKTLen) {
			bytesRead = fread(new_PKT->data, sizeof(char), rest_num_of_bytes, fp);
		} else {
			bytesRead = fread(new_PKT->data, sizeof(char), PKTLen, fp);
		}        
		if (bytesRead == -1) {
            fprintf(stderr, "File read failed!\n");
            exit(1);
        } else if (bytesRead == PKTLen){
	    	new_PKT->pktsize = PKTLen;
		} else {
	    	new_PKT->pktsize = bytesRead;
		}
		//fprintf(stderr, "bytesread = %d\n", bytesRead);
		//fprintf(stderr, "new_PKT.pktsize = %hi\n", new_PKT->pktsize);
        send_buf[seq] = new_PKT;
	// printf("This is the %d packet\n", send_buf[seq]->seq_num);
	// printf("Read %d Bytes.\n", bytesRead);
        seq += 1;
        rest_num_of_bytes -= bytesRead;
    }
		total_packets = seq;
        Packet *end_PKT = new Packet;
		memset((char*)end_PKT, 0, sizeof(*end_PKT));
		end_PKT->seq_num = -1;
		end_PKT->pktsize = total_packets - 1;
		send_buf[-1] = end_PKT;
		//fprintf(stderr, "total PKT = %d\n", total_packets);
		fclose(fp);
		return;
}

// 
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	
    tot_timeout = 0;
	
    readFileToBuffer(filename, bytesToTransfer);

    if (total_packets <= 0) {
	printf("Empty file!\n");
	return;
    }
    
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        fprintf(stderr, "socket creation failed!\n");
    
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &sender.timeout, sizeof(sender.timeout))== -1) {
		fprintf(stderr, "failed to set socket opt.!\n");
		exit(1);
	}

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

	/* Send data and receive acknowledgements on s*/

    // First, start in slow start phase
    sender.status = slow_start;
    ////set timer
    //auto last_send = std::chrono::high_resolution_clock::now();
    // enter the while loop

    while (!sender.complete) {
        // if the lact PKT received ACK, the process finished.
        if (sender.CWBase >= total_packets) {
            sender.complete = true;
            break;
        }

	// send the packets if CW has changed.
	int cw_upper_bound = sender.CWBase + std::floor(sender.CW);
	// Note: here curr_seq must less than upper bound! e.g: base=0, cw=5, we can send packet 0,1,2,3,4. Packet 5 cannot be sent.
	while (sender.curr_seq < total_packets && sender.curr_seq < cw_upper_bound) {
		send_PKT(sender.curr_seq);
    	auto this_send = std::chrono::high_resolution_clock::now();
      	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(this_send -start_time);
        PacketSendTime pktt;
        pktt.seq_num = sender.curr_seq;
        pktt.tv.tv_sec = duration.count() / 1000000;
        pktt.tv.tv_usec = duration.count() % 1000000;
        send_seq.push(pktt);
		sender.curr_seq++;
	}
        
		AckMessage *ACK = new AckMessage();
        struct sockaddr* from_addr = (struct sockaddr *)&si_other;
        socklen_t temp = slen;
        auto start = std::chrono::high_resolution_clock::now();
		//fprintf(stderr, "receiving ack, timeout = %d sec and %d usec\n", sender.timeout.tv_sec, sender.timeout.tv_usec);
        int recv_bytes = recvfrom(s, ACK, sizeof(*ACK), 0, (struct sockaddr *)&si_other, &temp);
		//fprintf(stderr, "Ack received\n");
        auto end = std::chrono::high_resolution_clock::now();

		

        if (recv_bytes == -1) {
            if (errno == ETIMEDOUT || errno == EAGAIN || errno == EWOULDBLOCK) {
                // timeout, fall in scary
				tot_timeout += 1;
				if (tot_timeout == 5) {
					fprintf(stderr, "serious timeout trouble\n");
					exit(1);
				}
				if (sender.CW <= 1) {
					sender.SSThreshold = 64;
				} else {
                	sender.SSThreshold = std::floor(sender.CW) / 2;
				}
                sender.CW = 1.0;
                sender.dup_ack_count = 0;
                sender.status = slow_start;
                send_PKT(sender.CWBase);
                sender.timeout.tv_sec = rto.tv_sec;
                sender.timeout.tv_usec = rto.tv_usec;
				//fprintf(stderr, "2  timeout = %d sec and %d usec\n", sender.timeout.tv_sec, sender.timeout.tv_usec);
				if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &sender.timeout, sizeof(sender.timeout))== -1) {
					fprintf(stderr, "failed to set socket opt.!\n");
					exit(1);
				}
                send_seq.empty();
                continue;
            } else {
                fprintf(stderr, "got error in ACK: %s\n", strerror(errno));
				exit(1);
            }
        } else {
			tot_timeout = 0;
			// update RTO
			auto receive_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start_time);
			//fprintf(stderr, "ack_num = %d and max_Ack = %d, and CWBase = %d\n", ACK->ack_num, max_ACK, sender.CWBase);
			if (ACK->ack_num == 0) {
				auto first_rtt = receive_time.count() - (send_seq.front().tv.tv_sec*1000000 + send_seq.front().tv.tv_usec);
				//fprintf(stderr, "first_rtt = %d usec\n", (int) first_rtt);
				srtt.tv_sec = first_rtt / 1000000;
				srtt.tv_usec = first_rtt % 1000000;
				//fprintf(stderr, "first srtt = %d sec and %d usec\n", srtt.tv_sec, srtt.tv_usec);
				rttval.tv_sec = (first_rtt / 2) / 1000000;
				rttval.tv_usec = (first_rtt / 2) % 1000000;
				//fprintf(stderr, "first rttval = %d sec and %d usec\n", rttval.tv_sec, rttval.tv_usec);
				auto rto_tot = first_rtt * 3;
				rto.tv_sec = rto_tot / 1000000;
				rto.tv_usec = rto_tot % 1000000;
				sender.timeout.tv_sec = rto.tv_sec;
				sender.timeout.tv_usec = rto.tv_usec;
				//fprintf(stderr, "3  timeout = %d sec and %d usec\n", sender.timeout.tv_sec, sender.timeout.tv_usec);
				if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &sender.timeout, sizeof(sender.timeout))== -1) {
					//fprintf(stderr, "failed to set socket opt.!\n");
					exit(1);
				}
			} else if (ACK->ack_num > max_ACK){
				if (send_seq.size() == 0) {
					struct timeval now;
					now.tv_sec = receive_time.count() / 1000000;
					now.tv_usec = receive_time.count() % 1000000;
					struct timeval prev_send = send_seq.front().tv;
					//fprintf(stderr, "before renew rto = %d sec and %d usec\n", rto.tv_sec, rto.tv_usec);
					calcu_new_rto(ACK->ack_num, now);
					sender.timeout.tv_sec = rto.tv_sec;
					sender.timeout.tv_usec = rto.tv_usec;
					if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &sender.timeout, sizeof(sender.timeout))== -1) {
						//fprintf(stderr, "failed to set socket opt.!\n");
						exit(1);
					}				
				}
			} else {
				auto timer_cost = std::chrono::duration_cast<std::chrono::microseconds>(start - end);
            	sender.timeout.tv_sec -= timer_cost.count() / 1000000;
            	sender.timeout.tv_usec -= timer_cost.count() % 1000000;
				if (sender.timeout.tv_usec >= 1000000) {
					sender.timeout.tv_sec += 1;
					sender.timeout.tv_usec -= 1000000;				
				}
				//fprintf(stderr, "5 timeout = %d sec and %d usec\n", sender.timeout.tv_sec, sender.timeout.tv_usec);
				if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &sender.timeout, sizeof(sender.timeout))== -1) {
					//fprintf(stderr, "failed to set socket opt.!\n");
					exit(1);
				}
			}
        }
        int ACK_seq = ACK->ack_num;
        if (ACK_seq == max_ACK) {
			//dupACK
            if (sender.status == slow_start) {
                sender.dup_ack_count += 1;
                if (sender.dup_ack_count == 3) {
					//fprintf(stderr, "enter fast recovery\n");
                    //fast recovery
                    sender.SSThreshold = std::floor(sender.CW / 2.0);
                    sender.CW = 1.0 * sender.SSThreshold + 3.0;
                    sender.status = fast_recovery;
					send_PKT(sender.CWBase);
                }
            } else if (sender.status == congestion_avoidance) {
                sender.dup_ack_count += 1;
                if (sender.dup_ack_count == 3) {
                    //fast recovery
                    sender.SSThreshold = std::floor(sender.CW / 2.0);
                    sender.CW = 1.0 * sender.SSThreshold + 3.0;
                    sender.status = fast_recovery;
					send_PKT(sender.CWBase);
                }
            } else if (sender.status == fast_recovery){
                sender.CW += 1.0;
            }
        } else if (ACK_seq >= sender.CWBase){
			
            if (sender.status == slow_start) {
				if ((ACK_seq - max_ACK) > (sender.SSThreshold - (int) sender.CW)) {
					int diff = ACK_seq - max_ACK - (sender.SSThreshold - (int) sender.CW);
					sender.CW = sender.SSThreshold;
					for (int i = 0; i < diff; i++) {
						sender.CW += 1.0/std::floor(sender.CW);
					}
					sender.CWBase = ACK_seq + 1;
                	sender.dup_ack_count = 0;
					sender.status = congestion_avoidance;
				} else {
					sender.CW += 1.0 * (ACK_seq-max_ACK);
					//fprintf(stderr, "===In slow start now, CW = %f\n", sender.CW);
                	sender.CWBase = ACK_seq + 1;
                	sender.dup_ack_count = 0;
                	if (std::floor(sender.CW) >= sender.SSThreshold) {
                    	// congestion_avoidance
                    	sender.status = congestion_avoidance;
                	}
				}
            } else if (sender.status == congestion_avoidance) {
                for (int i = 0; i < (ACK_seq - max_ACK); i++) {
					sender.CW += 1.0/std::floor(sender.CW);
				}
                sender.dup_ack_count = 0;
                sender.CWBase = ACK_seq + 1;
            } else if (sender.status == fast_recovery) {
                sender.CW = 1.0 * sender.SSThreshold;
                sender.dup_ack_count = 0;
                sender.CWBase = ACK_seq + 1;
				sender.status = congestion_avoidance;
				//fprintf(stderr, "back to congestion avoidance\n");
            }
			max_ACK = ACK_seq;
        }

    }
    for (int i = 0; i < 5; i++) {
        send_PKT(-1);
    }

    //printf("Closing the socket\n");
    close(s);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

	start_time = std::chrono::high_resolution_clock::now();
    unsigned short int udpPort;
    unsigned long long int numBytes;
	max_ACK = -1;
    slen = sizeof (si_other);
    if (argc != 5) {
        //fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

	sender.status = slow_start;
	sender.timeout.tv_sec = 0;
	sender.timeout.tv_usec = 200000; //microsceonds
    sender.dup_ack_count = 0;
	sender.curr_seq = 0;
	sender.CWBase = 0;
    sender.CW = 1.0;
    sender.SSThreshold = 55;
    sender.complete = false;
    
    
    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
	//fprintf(stderr, "end with CW = %f and SSThreshold = %d\n", sender.CW, sender.SSThreshold);

    return (EXIT_SUCCESS);
}


