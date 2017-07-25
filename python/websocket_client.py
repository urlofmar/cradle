import sys
import json
import websocket
import subprocess

server_process = None

if __name__ == "__main__":
    server_process = subprocess.Popen(["server"])

    ws = websocket.create_connection("ws://localhost:41072")
    ws.send(
        json.dumps(
            {
                "registration": {
                    "name": "Channing"
                }
            }))
    ws.send(
        json.dumps(
            {
                "test": {
                    "message": "Hello, Tigger!"
                }
            }))
    response = json.loads(ws.recv())["test"]
    if response["name"] != "Channing":
        sys.exit("name incorrect")
    if response["message"] != "Hello, Tigger!":
        sys.exit("message incorrect")
    ws.send(
        json.dumps(
            {
                "kill": None
            }))
    ws.close()

    if server_process.wait() != 0:
        sys.exit("server process failed")
