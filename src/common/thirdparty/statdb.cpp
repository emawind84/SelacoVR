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
    
    int val;
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
            strncpy(p.name, name.GetChars(), 64);
            
            if (stat.type == Stat::INT_TYPE) memcpy(&p.data, &stat.iVal, 4);
            else memcpy(&p.data, &stat.fVal, 4);

            outPackets.queue(p);
            stat.hasChanged = false;
        }

        for (auto& [name, ach] : allAchievements) {
            if (!ach.hasChanged) continue;

            StatPacket p;
            p.type = 3;
            strncpy(p.name, name.GetChars(), 64);
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
        search->second.hasIt = value;
        search->second.hasChanged = true;
        return true;
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

    while (mRunning.load() && rpcFailures < 50) {
        // Ensure connection, if failed reconnect
        if (!isConnected()) {
            connectRPC();
            rpcFailures++;
            std::this_thread::sleep_for(std::chrono::milliseconds(350));
        }
        else {
            rpcFailures = 0;
        }

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