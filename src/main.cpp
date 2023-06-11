#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <thread>
#include <filesystem>
#include <vector>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <lua.hpp>

#include "client.h"
#include "readmem.h"
#include "downloader.h"

using namespace std;

lua_State* L = luaL_newstate();

string autoSplittersDirectory;
string chosenAutoSplitter;
string ipAddress;
string processName;
vector<string> fileNames;

int pid = 0;

struct StockPid
{
    pid_t pid;
    char buff[512];
    FILE *pid_pipe;
} stockthepid;

void Func_StockPid(const char *processtarget)
{
    stockthepid.pid_pipe = popen(processtarget, "r");
    if (!fgets(stockthepid.buff, 512, stockthepid.pid_pipe))
    {
        cout << "Error reading process ID: " << strerror(errno) << endl;
    }

    stockthepid.pid = strtoul(stockthepid.buff, nullptr, 10);

    if (stockthepid.pid != 0)
    {
        cout << processName + " is running - PID NUMBER -> " << stockthepid.pid << endl;
        pclose(stockthepid.pid_pipe);
        pid = stockthepid.pid;
    }
    else {
        pclose(stockthepid.pid_pipe);
    }
}

void runAutoSplitter()
{
    luaL_dofile(L, chosenAutoSplitter.c_str());
    lua_close(L);
}

int processID(lua_State* L)
{
    processName = lua_tostring(L, 1);
    string command = "pidof " + processName;
    const char *cCommand = command.c_str();

    Func_StockPid(cCommand);
    while (pid == 0)
    {
        cout << processName + " isn't running. Retrying in 5 seconds...\n";
        sleep(5);
        system("clear");
        Func_StockPid(cCommand);
    }

    return 0;
}

int readAddress(lua_State* L)
{
    uint64_t address = lua_tointeger(L, 1) + lua_tointeger(L, 2);
    int addressSize = lua_tointeger(L, 3);
    uint64_t value;

    try
    {
        switch(addressSize)
        {
            case 8:
                value = readMem8(pid, address);
                break;
            case 32:
                value = readMem32(pid, address);
                break;
            case 64:
                value = readMem64(pid, address);
                break;
            default:
                cout << "Invalid address size. Please use 8, 32, or 64.\n";
                exit(-1);
        }
        lua_pushinteger(L, value);
    }
    catch (const std::exception& e)
    {
        cout << e.what() << endl;
        exit(-1);
    }


    this_thread::sleep_for(chrono::microseconds(1));

    return 1;
}

int sendCommand(lua_State* L)
{
    sendLSCommand(lua_tostring(L, 1));
    return 0;
}

void checkDirectories()
{
    string executablePath;
    string executableDirectory;

    // Get the path to the executable
    char result[ PATH_MAX ];
    ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
    executablePath = string( result, (count > 0) ? count : 0 );
    executableDirectory = executablePath.substr(0, executablePath.find_last_of("/"));

    autoSplittersDirectory = executableDirectory + "/autosplitters/";

    // Make the autosplitters directory if it doesn't exist
    if (!filesystem::exists(autoSplittersDirectory))
    {
        filesystem::create_directory(autoSplittersDirectory);
        startDownloader(autoSplittersDirectory);
    }


}

void chooseAutoSplitter()
{
    int counter = 1;
    for (const auto & entry : filesystem::directory_iterator(autoSplittersDirectory))
    {
        if (entry.path().extension() == ".lua")
        {
        cout << counter << ". " << entry.path().filename() << endl;
        fileNames.push_back(entry.path().string());
        counter++;
        }
    }

    switch (fileNames.size())
    {
        case 0:
        {
            cout << "No auto splitters found. Please put your auto splitters in the autosplitters folder or download some here.\n";
            startDownloader(autoSplittersDirectory);
            chooseAutoSplitter();
            break;
        }
        case 1:
        {
            chosenAutoSplitter = fileNames[0];
            break;
        }
        default:
        {
            int userChoice;
            cout << "Which auto splitter would you like to use? (Enter the number) ";
            cin >> userChoice;
            cin.ignore();
            chosenAutoSplitter = fileNames[userChoice - 1];
            break;
        }
    }
    cout <<  chosenAutoSplitter << endl;
}

void setIpAddress()
{
    cout << "What is your local IP address? (Leave blank for 127.0.0.1)\n";
    getline(std::cin, ipAddress);
    if (ipAddress.empty()) {
        ipAddress = "127.0.0.1";
    }
    Client(ipAddress);
}

int main(int argc, char *argv[])
{
    checkDirectories();
    
    luaL_openlibs(L);
    lua_pushcfunction(L, processID);
    lua_setglobal(L, "processID");
    lua_pushcfunction(L, readAddress);
    lua_setglobal(L, "readAddress");
    lua_pushcfunction(L, sendCommand);
    lua_setglobal(L, "sendCommand");

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "-downloader") == 0)
        {
            startDownloader(autoSplittersDirectory);
        }
    }

    chooseAutoSplitter();
    setIpAddress();
    runAutoSplitter();

    return 0;
}