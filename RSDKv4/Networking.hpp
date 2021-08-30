#if RETRO_USE_NETWORKING
#ifndef NETWORKING_H
#define NETWORKING_H
#include <thread>

extern char networkHost[64];
extern char networkGame[16];
extern int networkPort;

struct MultiplayerData {
    int type;
    int data[0x1FF];
};

struct CodedData {
    byte header;
    uint64_t code;
    uint roomcode;
    union {
        unsigned char bytes[0x1000];
        MultiplayerData multiData;
    } data;
};

class NetworkSession;

extern std::shared_ptr<NetworkSession> session;

void initNetwork();
void runNetwork();
void sendData();
void disconnectNetwork();
void waitForData(int type, int id, int slot);

void SetNetworkGameName(int *a1, const char *name);

#endif
#endif