#ifndef WebsocketManager_h
#define WebsocketManager_h

#include <Arduino.h>
#include <WebsocketsClient.h>

class WebsocketManager_
{
private:
    WebsocketManager_() = default;
public:
    static WebsocketManager_ &getInstance();
    void setup();
    void tick();

};

extern WebsocketManager_ &WebsocketManager;
#endif
