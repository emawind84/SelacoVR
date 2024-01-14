#include <iostream>
#include <thread>
#include <algorithm>
#include "statdb.h"
#include "printf.h"
#include "zstring.h"
#include "vm.h"
#include "c_cvars.h"

CUSTOM_CVAR(Bool, g_statdb, true, CVAR_ARCHIVE | CVAR_NOINITCALL) {
    Printf("The game will need to be restarted for this change to take effect.\n");
}

EXTERN_CVAR(Int, developer)

StatDatabase statDatabase;


DEFINE_ACTION_FUNCTION(_StatDatabase, GetStat)
{
    PARAM_PROLOGUE;
    PARAM_STRING(name);

    double flt = 0;
    bool success = statDatabase.getStat(name, &flt);

    if (numret > 0) ret[0].SetInt(success);
    if (numret > 1) ret[1].SetFloat(flt);

    return numret;
}


DEFINE_ACTION_FUNCTION(_StatDatabase, SetStat)
{
    PARAM_PROLOGUE;
    PARAM_STRING(name);
    PARAM_FLOAT(flt);

    bool success = statDatabase.setStat(name, flt);

    ACTION_RETURN_BOOL(success);
}


DEFINE_ACTION_FUNCTION(_StatDatabase, AddStat)
{
    PARAM_PROLOGUE;
    PARAM_STRING(name);
    PARAM_FLOAT(flt);
    double curVal = 0;
    bool success = false;

    if (flt > 0 && statDatabase.getStat(name, &curVal)) {
        statDatabase.setStat(name, flt + curVal);
    }

    ACTION_RETURN_BOOL(success);
}


DEFINE_ACTION_FUNCTION(_StatDatabase, GetAchievement)
{
    PARAM_PROLOGUE;
    PARAM_STRING(name);
    
    int val = 0;
    bool success = statDatabase.getAchievement(name, &val);

    if (numret > 0) ret[0].SetInt(success);
    if (numret > 1) ret[1].SetInt(val);

    return numret;
}


DEFINE_ACTION_FUNCTION(_StatDatabase, SetAchievement)
{
    PARAM_PROLOGUE;
    PARAM_STRING(name);
    PARAM_INT(val);

    bool success = statDatabase.setAchievement(name, val);

    ACTION_RETURN_BOOL(success);
}


DEFINE_ACTION_FUNCTION(_StatDatabase, IsAvailable)
{
    PARAM_PROLOGUE;
    ACTION_RETURN_BOOL(statDatabase.isAvailable());
}



StatDatabase::StatDatabase() {
    pipe = 0;
}

StatDatabase::~StatDatabase() {
    disconnect();
}

bool StatDatabase::isAvailable() {
    return mRunning.load();
}

void StatDatabase::start() {
    if (!g_statdb) return;  // Don't startup if disabled

    if (mThread.get_id() == std::thread::id()) {
        mRunning.store(true);
        mThread = std::thread(&StatDatabase::backgroundProc, this);
    }
}

void StatDatabase::update() {
    // Check for changed stats and create outgoing packets
    if (outPackets.size() < 32) {
        for (auto& [name, stat] : allStats) {
            if (!stat.hasChanged) continue;

            StatPacket p;
            p.type = (char)stat.type;
            strncpy(p.name, name.GetChars(), 63);
            
            if (stat.type == Stat::INT_TYPE) memcpy(&p.data, &stat.iVal, 4);
            else memcpy(&p.data, &stat.fVal, 4);

            outPackets.queue(p);
            stat.hasChanged = false;
        }

        for (auto& [name, ach] : allAchievements) {
            if (!ach.hasChanged) continue;

            StatPacket p;
            p.type = 3;
            strncpy(p.name, name.GetChars(), 63);
            memcpy(&p.data, &ach.hasIt, 4);

            outPackets.queue(p);
            ach.hasChanged = false;
        }
    }

    StatPacket p;
    while (inPackets.dequeue(p)) {
        // Update internal stats
        FString name(p.name);

        if (p.type == 3) {
            // Achievement
            auto& ach = allAchievements[name];
            ach.hasIt = *(reinterpret_cast<int32_t*>(p.data));
            ach.hasChanged = false;
            if (developer > 0) Printf("StatDatabase: Received Achievement - %s = %d\n", name.GetChars(), ach.hasIt);
        }
        else {
            // Stat
            auto& stat = allStats[name];
            if (stat.type == Stat::INT_TYPE && p.type == Stat::INT_TYPE) {
                stat.iVal = *(reinterpret_cast<int32_t*>(p.data));
                stat.hasChanged = false;
                if (developer > 0) Printf("StatDatabase: Received Stat - %s = %d\n", name.GetChars(), stat.iVal);
            }
            else if (stat.type == Stat::FLOAT_TYPE && p.type == Stat::FLOAT_TYPE) {
                stat.fVal = *(reinterpret_cast<float*>(p.data));
                stat.hasChanged = false;
                if (developer > 0) Printf("StatDatabase: Received Stat - %s = %f\n", name.GetChars(), stat.fVal);
            }
        }
    }
}

void StatDatabase::disconnect() {
    // Make sure thread is killed
    if (mThread.joinable()) {
        mRunning.store(false);
        mWake.notify_all();
        mThread.join();
    }
}

bool StatDatabase::setStat(FString name, double value) {
    if (auto search = allStats.find(name); search != allStats.end()) {
        if (search->second.type == Stat::INT_TYPE) {
            int32_t val = (int)std::round(value);
            if (search->second.iVal != val) {
                search->second.iVal = val;
                search->second.hasChanged = true;
                return true;
            }
        }
        else if (search->second.type == Stat::FLOAT_TYPE) {
            float val = (float)value;
            if (search->second.fVal != val) {
                search->second.fVal = val;
                search->second.hasChanged = true;
                return true;
            }
        }
    }

    return false;
}

bool StatDatabase::setAchievement(FString name, int value) {
    if (auto search = allAchievements.find(name); search != allAchievements.end()) {
        if(value != search->second.hasIt) {
            search->second.hasIt = value;
            search->second.hasChanged = true;
            return true;
        }
    }
    return false;
}

bool StatDatabase::getStat(FString name, double* value) {
    if (value == nullptr) return false;
    if (auto search = allStats.find(name); search != allStats.end()) {
        *value = search->second.type == Stat::INT_TYPE ? (double)search->second.iVal : (double)search->second.fVal;
        return true;
    }
    return false;
}

bool StatDatabase::getAchievement(FString name, int* value) {
    if (value == nullptr) return false;
    if (auto search = allAchievements.find(name); search != allAchievements.end()) {
        *value = search->second.hasIt;
        return true;
    }
    return false;
}


void StatDatabase::backgroundProc() {
    std::unique_lock<std::mutex> lock(mWakeLock);

    rpcFailures = connectRPC() ? 0 : 1;
    bool sentRequestForData = false;
    auto lastKeepAlive = std::chrono::steady_clock::now();

    while (mRunning.load() && rpcFailures < 75) {
        // Ensure connection, if failed reconnect
        if (!isConnected()) {
            connectRPC();
            rpcFailures++;
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
        }
        else {
            rpcFailures = 0;
        }

        
        if(isConnected()) {
            // Check for more data from server
            StatPacket p;
            while (readRPC(&p, sizeof(StatPacket))) {
                p.name[63] = '\0';
                if(p.type <= 3 && p.type >= 0) inPackets.queue(p);
            }

            // If we have new data, send it
            while (outPackets.dequeue(p)) {
                if (!writeRPC(&p, sizeof(StatPacket))) {
                    break;
                }
            }

            if(!sentRequestForData) {
                StatPacket p;
                p.type = 4;
                if(writeRPC(&p, sizeof(StatPacket))) {
                    sentRequestForData = true;
                }
            }

            auto keepAliveTimer = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastKeepAlive);
            if(keepAliveTimer.count() > 500) {
                // Send keep alive
                StatPacket p;
                p.type = 5;
                if(writeRPC(&p, sizeof(StatPacket))) {
                    lastKeepAlive = std::chrono::steady_clock::now();
                } else {
                    rpcFailures++;
                }
            }
        }
        
        mWake.wait_for(lock, std::chrono::milliseconds(75));
    }

    mRunning.store(false);
    disconnectRPC();
}



#ifdef WIN32
#include <Windows.h>

void StatDatabase::init() {
    pipe = INVALID_HANDLE_VALUE;
}

bool StatDatabase::isConnected() {
    return pipe != INVALID_HANDLE_VALUE;
}

bool StatDatabase::connectRPC() {
    wchar_t name[] = TEXT("\\\\.\\pipe\\selacostats");

    while (pipe == INVALID_HANDLE_VALUE) {
        pipe = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

        if (pipe == INVALID_HANDLE_VALUE) {
            auto lastError = GetLastError();
            if (lastError == ERROR_FILE_NOT_FOUND) {
                return false;
            }
            else if (lastError == ERROR_PIPE_BUSY) {
                if (!WaitNamedPipeW(name, 500)) return false;
            }
        }
    }

    return pipe != INVALID_HANDLE_VALUE;
}

void StatDatabase::disconnectRPC() {
    CloseHandle(pipe);
    pipe = INVALID_HANDLE_VALUE;
}


bool StatDatabase::writeRPC(const void* data, size_t size) {
    if (!size || data == nullptr) return false;

    DWORD bytes = (DWORD)size;
    DWORD numWritten = 0;

    return WriteFile(pipe, data, bytes, &numWritten, nullptr) && numWritten == size;
}


bool StatDatabase::readRPC(void* data, size_t size) {
    if (data == nullptr || size == 0) return false;

    DWORD avail = 0;
    DWORD readSize = (DWORD)size;

    if (PeekNamedPipe(pipe, nullptr, 0, nullptr, &avail, nullptr)) {
        if (avail >= readSize) {
            DWORD numRead = 0;

            if (ReadFile(pipe, data, readSize, &numRead, nullptr)) {
                return true;
            }
            else 
                disconnectRPC();
        }
    }
    else 
        disconnectRPC();

    return false;
}

#endif // WIN32



#ifdef __linux__

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#define OUT_PIPE "/var/lock/selacoStat2"
#define IN_PIPE "/var/lock/selacoStat1"

int outputFile = -1;
int inputFile = -1;
bool outReady = false;

void StatDatabase::init() {
    
}

bool StatDatabase::isConnected() {
    return (inputFile > -1) && (outputFile > -1) && outReady;
}

bool StatDatabase::connectRPC() {
    if(inputFile <= 0) {
        inputFile = open(IN_PIPE, O_RDONLY | O_NONBLOCK);
        if(inputFile < 0) {
            return false;
        }

        // Wait briefly for a connection
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(inputFile, &readfds);
        timeval tw = { 0, 1000 * 100 };
        auto con2 = select(inputFile + 1, &readfds, NULL, NULL, &tw);
        if(con2 < 0) {
            return false;
        }
    }


    if(outputFile < 0) {
        // Reading works, let's set up writing
        unlink(OUT_PIPE);
        int con = mkfifo(OUT_PIPE, 0666);

        if(outputFile < 0) {
            outputFile = open(OUT_PIPE, O_RDWR | O_NONBLOCK);
            if(outputFile < 0){
                unlink(OUT_PIPE);
                return false;
            }
        }
    }
    
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(outputFile, &writefds);
    timeval tw = { 0, 1000 * 100 };
    auto con2 = select(outputFile + 1, &writefds, NULL, NULL, &tw);
    if(con2 < 0) {
        return false;
    }

    outReady = true;

    return true;
}

void StatDatabase::disconnectRPC() {
    close(outputFile);
    close(inputFile);
    unlink(OUT_PIPE);
    outputFile = -1;
    inputFile = -1;
    outReady = false;
}


bool StatDatabase::writeRPC(const void* data, size_t size) {
    if (!size || data == nullptr || !outReady) return false;

    auto ret = write(outputFile, data, size);
    if(ret < 0) {
        // write is no longer working, invalidate outfile
        close(outputFile);
        outReady = false;
        outputFile = -1;
        return false;
    }

    return ret == (ssize_t)size;
}


bool StatDatabase::readRPC(void* data, size_t size) {
    if (data == nullptr || size == 0) return false;

    auto ret = read(inputFile, data, size);
    if(ret < 0) {
        if(errno != EAGAIN) {
            const char *err = strerror(errno);
            Printf("Error: %s", err);
            close(inputFile);
            inputFile = -1;
        }
        return false;
    }

    return ret == (ssize_t)size;
}

#endif 