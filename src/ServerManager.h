#ifndef ServerManager_h
#define ServerManager_h

#include <Arduino.h>

class ServerManager_
{
private:
    ServerManager_() = default;
    void handleExternalApi();
    uint32_t lastExternalApiCall = 0;

public:
    static ServerManager_ &getInstance();
    void setup();
    void tick();
    void loadSettings();
    void sendButton(byte btn, bool state);
    void erase();
    bool isConnected;
    IPAddress myIP;
};

extern ServerManager_ &ServerManager;
 
#endif
