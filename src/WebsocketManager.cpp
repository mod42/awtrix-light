#include <WebSocketsClient.h>

#include <WebsocketManager.h>
#include "Globals.h"

const char* webSocketServerAddress = "192.168.0.129";
const uint16_t webSocketServerPort = 8082;

WebSocketsClient client;

WebsocketManager_ &WebsocketManager_::getInstance()
{
    static WebsocketManager_ instance;
    return instance;
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

  DEBUG_PRINTF("Got a message");

  DEBUG_PRINTF("\nCHIP MAC: %012llx\n", ESP.getEfuseMac());

	switch(type) {
		case WStype_DISCONNECTED:
			 DEBUG_PRINTLN(F("[WSc] Disconnected!\n"));
			break;
		case WStype_CONNECTED:
			DEBUG_PRINTF("[WSc] Connected to url: %s\n", webSocketServerAddress);

			// send message to server when Connected
			client.sendTXT("Connected");
			break;
		case WStype_TEXT:
			DEBUG_PRINTF("[WSc] get text: %s\n", payload);

			// send message to server
			// webSocket.sendTXT("message here");
			break;
		case WStype_BIN:
			DEBUG_PRINTF("[WSc] get binary length: %u\n", length);
			//hexdump(payload, length);

			// send data to server
			// webSocket.sendBIN(payload, length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}
}


// Initialize the global shared instance
WebsocketManager_ &WebsocketManager = WebsocketManager.getInstance();

void WebsocketManager_::setup(){
      //if (DEBUG_MODE)
  DEBUG_PRINTLN(F("Setup websocket"));
  client.begin(webSocketServerAddress,webSocketServerPort,"/", "ws");
  DEBUG_PRINTLN(F("after client begin"));
  client.onEvent(webSocketEvent);
  DEBUG_PRINTLN(F("set onEvent"));
} 

void WebsocketManager_::tick(){
  client.loop();

}
