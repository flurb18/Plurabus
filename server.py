#!/usr/bin/env python

import asyncio
import os
import signal
import websockets

SocketQueue = []

async def trySend(socket, data):
    try:
        await socket.send(data)
        return True
    except ConnectionClosed:
        print("Host "+str(socket.remote_address[0])+" connection closed")
        return False
    except Exception as e:
        print("Host "+str(socket.remote_address[0])+" Send failed")
        return False
async def tryRecv(socket):
    try:
        data = await socket.recv()
        return data
    except ConnectionClosed:
        print("Host "+str(socket.remote_address[0])+" connection closed")
        return False
    except Exception as e:
        print("Host "+str(socket.remote_address[0])+" Recv failed")
        return False

async def serveFunction(websocket):
    print("Incoming connection, host " + str(websocket.remote_address[0]) +
          " port " + str(websocket.remote_address[1]))
    websocket.desiredPairedString = await tryRecv(websocket)
    if (not isinstance(websocket.desiredPairedString,str)):
        await websocket.close()
        return
    print("Desired pair string: " + websocket.desiredPairedString)
    websocket.foundPartner = False
    websocket.readyForGame = asyncio.Event()
    for waitingClient in SocketQueue:
        if (waitingClient.desiredPairedString == websocket.desiredPairedString):
            websocket.pairedClient = waitingClient
            waitingClient.pairedClient = websocket
            websocket.player = 1
            waitingClient.player = 2
            SocketQueue.remove(waitingClient)
            waitingClient.foundPartner = True
            websocket.foundPartner = True
            break

    if (not websocket.foundPartner):
        SocketQueue.append(websocket)
        try:
            ready = await websocket.recv()
        except websockets.exceptions.ConnectionClosed:
            SocketQueue.remove(websocket)
            return
        except Exception as e:
            SocketQueue.remove(websocket)
            return

    else:
        await trySend(websocket, str(websocket.pairedClient.desiredPairedString))
        await trySend(websocket.pairedClient, str(websocket.desiredPairedString))
        ready = await tryRecv(websocket)

    # threads merge here
    if (websocket.player == 1):
        await trySend(websocket, "P1")
    if (websocket.player == 2):
        await trySend(websocket, "P2")

    set1 = await tryRecv(websocket)
    websocket.readyForGame.set()
    await websocket.pairedClient.readyForGame.wait()
    if (websocket.player == 2):
        await websocket.wait_closed()
        
    if (websocket.player == 1):
        await trySend(websocket, "Go")
        
        async for data in websocket:
            try:
                await websocket.pairedClient.send(data)
            except websockets.exceptions.ConnectionClosed:
                print("Host "+str(websocket.pairedClient.remote_address[0])+" connection closed")
                if (websocket.pairedClient.close_code == 1001):
                    await websocket.close(1001, "Other player disconnected.")
                    
            try:
                otherdata = await websocket.pairedClient.recv()
            except websockets.exceptions.ConnectionClosed:
                print("Host "+str(websocket.pairedClient.remote_address[0])+" connection closed")
                if (websocket.pairedClient.close_code == 1001):
                    await websocket.close(1001, "Other player disconnected.")

            try:
                await websocket.send(otherdata)
            except websockets.exceptions.ConnectionClosed:
                print("Host "+str(websocket.remote_address[0])+" connection closed")
                if (websocket.close_code == 1001):
                    await websocket.pairedClient.close(1001, "Other player disconnected.")

    if (websocket.close_code == 1001):
        await websocket.pairedClient.close(1001, "Other player disconnected.")

async def main():
    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    async with websockets.serve(
            serveFunction,
            host="",
            port=31108,
            reuse_port=True
    ):
        await stop
        
if __name__ == "__main__":
    asyncio.run(main())
