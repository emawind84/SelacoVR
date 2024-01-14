#ifndef __STATSDB_H__
#define __STATSDB_H__


#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <mutex>

#include "zstring.h"
#include "TSQueue.h"

class StatDatabase {
public:
    struct Stat {
        enum STAT_TYPE {
            INT_TYPE = 0,
            FLOAT_TYPE = 1
        } type = INT_TYPE;
        float fVal = 0;
        int32_t iVal = 0;
        bool hasChanged = false;
    };

    struct Achievement {
        int32_t hasIt = 0;
        bool hasChanged = false;
    };


	StatDatabase();
    ~StatDatabase();

    void update();

    bool isAvailable();
    void init();

    void start();
    void disconnect();

    bool setStat(FString name, double value);
    bool setAchievement(FString name, int value);

    bool getStat(FString name, double *value);
    bool getAchievement(FString name, int* value);

protected:
    struct StatPacket {
        char name[64];
        char type;
        unsigned char data[4];
    };

    TSQueue<StatPacket> outPackets;
    TSQueue<StatPacket> inPackets;

    std::map<FString, Stat> allStats;
    std::map<FString, Achievement> allAchievements;
    
    std::mutex mWakeLock;
    std::thread mThread;
    std::condition_variable mWake;
    std::atomic<bool> mRunning{ false };

    int rpcFailures = 0;

    void backgroundProc();
    bool connectRPC();
    bool writeRPC(const void* data, size_t size);
    bool readRPC(void* data, size_t size);
    void disconnectRPC();
    bool isConnected();

    void* pipe;
};

extern StatDatabase statDatabase;


#endif