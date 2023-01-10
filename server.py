#!/usr/bin/env python

import asyncio
import os
import signal
import websockets
import ssl

SocketQueue = []
FRAME_DELAY = 0.004

async def timerLoop(websocket):
    while (True):
        await asyncio.sleep(1)
        try:
            await websocket.send("a");
            await websocket.pairedClient.send("a");
        except websockets.exceptions.ConnectionClosed:
            return
        except Exception as e:
            return
    
async def serveFunction(websocket):
    websocket.desiredPairedString = await websocket.recv()
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
        try:
            await websocket.send(str(websocket.pairedClient.desiredPairedString))
            await websocket.pairedClient.send(str(websocket.desiredPairedString))
            ready = await websocket.recv()
        except websockets.exceptions.ConnectionClosed:
            await websocket.close()
            await websocket.pairedClient.close()
            return
        except Exception as e:
            await websocket.close()
            await websocket.pairedClient.close()
            return
            
    # Threads merge
    try:
        if (websocket.player == 1):
            await websocket.send("P1")
        if (websocket.player == 2):
            await websocket.send("P2")
        set1 = await websocket.recv()
    except websockets.exceptions.ConnectionClosed:
        await websocket.pairedClient.close()
        return

    websocket.readyForGame.set()
    await websocket.pairedClient.readyForGame.wait()

    if (websocket.player == 2):
        async for data in websocket:
            await asyncio.sleep(FRAME_DELAY)
            try:
                await websocket.pairedClient.send(data)
            except websockets.exceptions.ConnectionClosed:
                await websocket.close(1001, "Other player disconnected.")
            except Exception as e:
                await websocket.close(1001, "")
        if (websocket.close_code == 1001):
            await websocket.pairedClient.close(1001, "Other player disconnected.")
        
    if (websocket.player == 1):
        try:
            await websocket.send("Go")
            start = await websocket.recv()
        except websockets.exceptions.ConnectionClosed:
            await websocket.pairedClient.close()
            return
        asyncio.ensure_future(timerLoop(websocket))
        async for data in websocket:
            await asyncio.sleep(FRAME_DELAY)
            try:
                await websocket.pairedClient.send(data)
            except websockets.exceptions.ConnectionClosed:
                await websocket.close(1001, "Other player disconnected.")
            except Exception as e:
                await websocket.close(1001, "")
        if (websocket.close_code == 1001):
            await websocket.pairedClient.close(1001, "Other player disconnected.")

async def main():
    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain('/etc/ssl/certs/selfsigned3.pem')
    portenv = int(os.environ.get("PORT", "31108"))
    async with websockets.serve(
            serveFunction,
            host="10.8.0.1",
            port=portenv,
            reuse_port=True,
            ssl=ssl_context
    ):
        await stop
        
if __name__ == "__main__":
    asyncio.run(main())
