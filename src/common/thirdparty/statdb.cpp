#include <iostream>
#include <thread>
#include <algorithm>
#include "statdb.h"
#include "printf.h"
#include "zstring.h"
#include "vm.h"
#include "c_cvars.h"
#include "c_dispatch.h"

CUSTOM_CVAR(Bool, g_statdb, false, CVAR_NOSET | CVAR_NOINITCALL) {
    Printf("This value can only be changed from the command line.\n");
}

EXTERN_CVAR(Bool, sv_cheats)
EXTERN_CVAR(Int, developer)

StatDatabase statDatabase;

#define ALLOW_STAT_CHEATS


#ifdef ALLOW_STAT_CHEATS

CCMD(globalstats)
{
    statDatabase.print();
}

#endif


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

#ifndef ALLOW_STAT_CHEATS
    if (sv_cheats) {
        if (developer > 0) {
            Printf(TEXTCOLOR_RED"StatDatabase::SetStat() not available with sv_cheats enabled.\n");
        }
        return false;
    }
#endif

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

#ifndef ALLOW_STAT_CHEATS
    if (sv_cheats) {
        if (developer > 0) {
            Printf(TEXTCOLOR_RED"StatDatabase::AddStat() not available with sv_cheats enabled.\n");
        }
        return false;
    }
#endif

    if (flt > 0 && statDatabase.getStat(name, &curVal)) {
        success = statDatabase.setStat(name, flt + curVal);
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

#ifndef ALLOW_STAT_CHEATS
    if (sv_cheats) {
        if (developer > 0) {
            Printf(TEXTCOLOR_RED"StatDatabase::SetAchievement() not available with sv_cheats enabled.\n");
        }
        return false;
    }
#endif

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

void StatDatabase::print() {
    Printf("\nAll Stats\n----------------\n");
    for (auto& [name, stat] : allStats) {
        if (stat.type == Stat::FLOAT_TYPE)
            Printf("%s : %.4f\n", name.GetChars(), stat.fVal);
        else
            Printf("%s : %d\n", name.GetChars(), stat.iVal);
    }

    Printf("\nAll Achievements\n----------------\n");
    for (auto& [name, ach] : allAchievements) {
        if (ach.hasIt)
            Printf(TEXTCOLOR_GOLD "%s : (COMPLETE)\n", name.GetChars());
        else
            Printf("%s\n", name.GetChars());
    }
}

bool StatDatabase::isAvailable() {
    return g_statdb && allStats.size() > 0 && mRunning.load();
}

void StatDatabase::start() {
    if (!g_statdb) return;  // Don't startup if disabled

    if (mThread.get_id() == std::thread::id()) {
        mRunning.store(true);
        mThread = std::thread(&StatDatabase::backgroundProc, this);
    }

    lastStatUpdate = std::chrono::steady_clock::now();
}

void StatDatabase::update() {
    if (!g_statdb) return;

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
        else if(p.type == Stat::FLOAT_TYPE || p.type == Stat::INT_TYPE) {
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
        } else if(developer > 0) {
            Printf("StatDatabase: Received nonsense: %d - %s", p.type, p.name);
        }
    }

    // Only run output updates every half second or so, or if there are new achievements
    if(!newAchievements && (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastStatUpdate).count() < 500)) {
        return;
    }

    // Check for changed stats and create outgoing packets
    if (outPackets.size() < 32) {
        queueOutput();

        newAchievements = false;
    }
}

void StatDatabase::queueOutput() {
    for (auto& [name, stat] : allStats) {
        if (!stat.hasChanged) continue;

        StatPacket p;
        p.type = (char)stat.type;
        strncpy(p.name, name.GetChars(), 63);
        
        if (stat.type == Stat::INT_TYPE) memcpy(&p.data, &stat.iVal, 4);
        else memcpy(&p.data, &stat.fVal, 4);

        outPackets.queue(p);
        stat.hasChanged = false;
        if(developer > 0 && stat.type == Stat::INT_TYPE) Printf("StatDatabase: Queued %s : %d\n", name.GetChars(), stat.iVal);
        else if(developer > 0) Printf("StatDatabase: Queued %s : %f\n", name.GetChars(), stat.fVal);
    }

    for (auto& [name, ach] : allAchievements) {
        if (!ach.hasChanged) continue;

        StatPacket p;
        p.type = 3;
        strncpy(p.name, name.GetChars(), 63);
        memcpy(&p.data, &ach.hasIt, 4);

        outPackets.queue(p);
        ach.hasChanged = false;

        if(developer > 0) Printf("StatDatabase: Queued Achievement %s", name.GetChars());
    }
}

void StatDatabase::disconnect() {
    // Make sure thread is killed
    if (mThread.joinable()) {
        queueOutput(); // Queue the last of the output

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
            newAchievements = true;
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

    while (rpcFailures < 75) {
        // Ensure connection, if failed reconnect
        if (!isConnected() && mRunning.load()) {
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

        if(!mRunning.load())
            break;
        
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
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/un.h>

#define OUT_PIPE "/var/lock/selacoStat2"
#define IN_PIPE "/var/lock/selacoStat1"

int sock = -1;
int outputFile = -1;
int inputFile = -1;
bool outReady = false;

void StatDatabase::init() {
    
}

bool StatDatabase::isConnected() {
    return sock > -1 && outReady;
}

bool StatDatabase::connectRPC() {
    if(sock < 0) {
        sock = socket(AF_LOCAL, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if(sock < 0) {
            return false;
        }

        struct hostent* hptr = gethostbyname("127.0.0.1");
        if (!hptr || (hptr->h_addrtype != AF_LOCAL && hptr->h_addrtype != AF_INET))  {
            close(sock);
            sock = -1;
            return false;
        }

        bool bindSuccess = false;

        struct sockaddr_un saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sun_family = AF_LOCAL;
        strncpy(saddr.sun_path, IN_PIPE, sizeof(saddr.sun_path) - 1);

        if(connect(sock, (struct sockaddr*) &saddr, sizeof(saddr)) < 0) {
            close(sock);
            return false;
        }

        outReady = true;
        return true;
    }

    return false;
}

void StatDatabase::disconnectRPC() {
    close(sock);
    outReady = false;
}


bool StatDatabase::writeRPC(const void* data, size_t size) {
    if (!size || data == nullptr || !outReady) return false;

    auto ret = write(sock, data, size);
    if(ret < 0) {
        // write is no longer working, invalidate outfile
        close(sock);
        outReady = false;
        sock = -1;
        return false;
    }

    return ret == (ssize_t)size;
}


bool StatDatabase::readRPC(void* data, size_t size) {
    if (data == nullptr || size == 0) return false;

    auto ret = read(sock, data, size);
    if(ret < 0) {
        if(errno != EAGAIN) {
            const char *err = strerror(errno);
            close(sock);
            sock = -1;
        }
        return false;
    }

    return ret == (ssize_t)size;
}

#endif 