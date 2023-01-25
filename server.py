#!/usr/bin/env python

import argparse
import asyncio
import os
import signal
import websockets
import ssl
import urllib.parse
import http
import pathlib
import uuid
import mariadb
import sys

CONTENT_TYPES = {
    ".css": "text/css",
    ".html": "text/html; charset=utf-8",
    ".ico": "image/x-icon",
    ".js": "text/javascript",
    ".wasm": "application/wasm",
    ".data": "binary"
}

wssPort = 31108
httpPort = 31107
hostName = "10.8.0.1"
certChain = "/etc/ssl/certs/selfsigned3.pem"

Tokens = []
SocketQueue = []
FRAME_DELAY = 0.010

def create_token(lifetime=5):
    token = uuid.uuid4().hex
    Tokens.append(token)
    asyncio.get_running_loop().call_later(lifetime, Tokens.remove, token)
    return token

def isValidToken(token):
    if (token in Tokens):
        return True
    else:
        return False

def isUser(cur, username):
    cur.execute(
        "SELECT EXISTS(SELECT 1 FROM mysql.user WHERE User=?)",
        (username,))
    ret = cur.fetchall()
    return (ret[0][0] == 1)

def isAuthorizedUser(cur, username, password):
    cur.execute(
        "SELECT EXISTS(SELECT 1 FROM mysql.user WHERE User=? AND Password=PASSWORD(?))",
        (username, password))
    ret = cur.fetchall()
    return (ret[0][0] == 1)
    
async def serve_html(path, request_headers):
    path = urllib.parse.urlparse(path).path
    if path == "/" or path == "":
        page = "index.html"
    else:
        page = path[1:]
    try:
        p = pathlib.Path(__file__).resolve()
        template = p.parent.joinpath("web").joinpath(page)
    except ValueError:
        pass
    else:
        if template.is_file():
            headers = {"Content-Type": CONTENT_TYPES[template.suffix]}
            body = template.read_bytes()
            if (template.name == 'hivemindweb.wasm'):
                token = create_token()
                body = body.replace(b"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", token.encode())
            return http.HTTPStatus.OK, headers, body

    return http.HTTPStatus.NOT_FOUND, {}, b"Not found\n"

async def noop_handler(websocket):
    pass

async def timerLoop(websocket):
    while (True):
        await asyncio.sleep(1)
        try:
            await websocket.send("TIMER");
            await websocket.pairedClient.send("TIMER");
        except websockets.exceptions.ConnectionClosed:
            return
        except Exception as e:
            return
    
async def serve_wss(websocket):
    try:
        firstMessage = await websocket.recv()
    except websockets.exceptions.ConnectionClosed:
        return
    tokenValid = isValidToken(firstMessage)
    valid = tokenValid
    if (not valid):
        if (isUser(conn.cursor(), firstMessage)):
            try:
                passwd = await websocket.recv()
            except websockets.exceptions.ConnectionClosed:
                return
            valid = isAuthorizedUser(conn.cursor(), firstMessage, passwd)
    if (not valid):
        await websocket.close(1011, "Invalid token")
        return
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
        if (isinstance(data, str)):
            if (data == "RESIGN"):
                try:
                    await websocket.pairedClient.send("RESIGN")
                    await websocket.pairedClient.wait_closed()
                    await websocket.wait_closed()
                    return
                except websockets.exceptions.ConnectionClosed:
                    await websocket.wait_closed()
                    return
        try:
            await websocket.pairedClient.send(data)
        except websockets.exceptions.ConnectionClosed:
            try:
                await websocket.send("DISCONNECT")
                await websocket.wait_closed()
                return
            except websockets.exceptions.ConnectionClosed:
                return
        except Exception as e:
            await websocket.close()
            await websocket.pairedClient.close()

    if (websocket.close_code == 1001):
        try:
            await websocket.pairedClient.send("DISCONNECT")
            await websocket.pairedClient.wait_closed()
        except websockets.exceptions.ConnectionClosed:
            pass

async def main():
    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    ssl_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_context.load_cert_chain(certChain)
    htmlSocket = str(pathlib.Path(__file__).resolve().parent.joinpath("web.sock"))
    wssSocket = str(pathlib.Path(__file__).resolve().parent.joinpath("wss.sock"))
    async with websockets.unix_serve(
            noop_handler,
            path=htmlSocket,
            process_request=serve_html
    ), websockets.unix_serve(
        serve_wss,
        path=wssSocket,
        ssl=ssl_context
    ):
        await stop


        
if __name__ == "__main__":
    try:
        conn = mariadb.connect(
            user="server",
            unix_socket="/run/mysqld/mysqld.sock"
        )
    except mariadb.Error as e:
        print(f"Error connecting to MariaDB Platform: {e}")
        sys.exit(1)
    asyncio.run(main())
