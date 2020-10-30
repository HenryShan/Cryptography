/* 
 * File:  dataStructure.h
 * Purpose: common data structures
 */

#define PKTLen 1440

struct Packet{
    int seq_num;
    short pktsize;
    char data[PKTLen];
};

struct AckMessage{
    int ack_num;
};

struct PacketSendTime{
    int seq_num;
    struct timeval tv;
};
