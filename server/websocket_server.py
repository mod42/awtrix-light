import asyncio
import websockets
import json
import time
import requests
import logging

logger = logging.getLogger('websockets')
logger.setLevel(logging.DEBUG)
logger.addHandler(logging.StreamHandler())

async def handle_websocket(websocket, path):
    try:
        async for message in websocket:
            try:
                # Parse the incoming JSON message from the client
                request = json.loads(message)
                lang = request.get("lang", "en_us")
                token = request.get("token", "")
                service = request.get("service", "")

                # Handle different services requested by the client
                if service == "connect":
                    print("connect received")
                    # Simulate connecting to the WebSocket and retrieving token
                    token = "sample_token"  # Replace with actual token retrieval logic
                    response = {"result_msg": "success", "result_data": {"token": token}}
                elif service == "devicelist":
                    print("connect devicelist")
                    # Simulate retrieving device information
                    # Replace with actual device information retrieval logic
                    dev_type = "sample_dev_type"
                    dev_code = "sample_dev_code"
                    response = {"result_msg": "success", "result_data": {"dev_type": dev_type, "dev_code": dev_code}}
                else:
                    print("else received")
                    response = {"result_msg": "error", "error_msg": "Unknown service"}

                # Send response back to the client
                await websocket.send(json.dumps(response))
            except Exception as e:
                # Handle any exceptions and send error response back to the client
                error_response = {"result_msg": "error", "error_msg": str(e)}
                await websocket.send(json.dumps(error_response))

    except websockets.exceptions.ConnectionClosed:
        print("Client disconnected")

async def main():
    # Set up the WebSocket server
    server = await websockets.serve(handle_websocket, "0.0.0.0", 8082)

    print("server started")
    # Start the server
    await server.wait_closed()

if __name__ == "__main__":
    asyncio.run(main())
