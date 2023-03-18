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
import sys
import secrets
import json
from google.cloud import recaptchaenterprise_v1
from google.cloud.recaptchaenterprise_v1 import Assessment

CONTENT_TYPES = {
    ".css": "text/css",
    ".html": "text/html; charset=utf-8",
    ".ico": "image/x-icon",
    ".png": "image/png",
    ".js": "text/javascript",
    ".wasm": "application/wasm",
    ".data": "binary"
}

if (len(sys.argv) < 2):
    print("Input host name as command argument")
    exit()

hostName = sys.argv[1]

recaptchaSiteKey = "6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD"
projectID = "skillful-garden-379804"

Tokens = []
SocketQueue = []
LobbyKeys = []
FRAME_DELAY = 0.010

def create_token(lifetime=5):
    token = uuid.uuid4().hex
    Tokens.append(token)
    asyncio.get_running_loop().call_later(lifetime, Tokens.remove, token)
    return token

def create_gamekey(lifetime=180):
    gameKey = secrets.token_urlsafe(12)
    LobbyKeys.append(gameKey)
    asyncio.get_running_loop().call_later(lifetime, LobbyKeys.remove, gameKey)
    return gameKey

# From https://cloud.google.com/recaptcha-enterprise/docs/create-assessment
def create_assessment(
        project_id: str, recaptcha_site_key: str, token: str, recaptcha_action: str
) -> Assessment:
    """Create an assessment to analyze the risk of a UI action.
    Args:
        project_id: GCloud Project ID
        recaptcha_site_key: Site key obtained by registering a domain/app to use recaptcha services.
        token: The token obtained from the client on passing the recaptchaSiteKey.
        recaptcha_action: Action name corresponding to the token.
    """

    client = recaptchaenterprise_v1.RecaptchaEnterpriseServiceClient()

    # Set the properties of the event to be tracked.
    event = recaptchaenterprise_v1.Event()
    event.site_key = recaptcha_site_key
    event.token = token

    assessment = recaptchaenterprise_v1.Assessment()
    assessment.event = event

    project_name = f"projects/{project_id}"

    # Build the assessment request.
    request = recaptchaenterprise_v1.CreateAssessmentRequest()
    request.assessment = assessment
    request.parent = project_name

    response = client.create_assessment(request)
    return response

async def serve_html(requrl, request_headers):
    parsed_url = urllib.parse.urlparse(requrl)
    path = parsed_url.path
    pstr = "default"
    token = "default"
    lobbyKey = "default"
    if path == "/" or path == "":
        page = "index.html"
    else:
        page = path[1:]
        if (page == "play.html" or page == "private.html"):
            page = "index.html"
        if (len(page) > 50):
            return http.HTTPStatus.REQUEST_URI_TOO_LONG, {}, b"Request URI too long\n"
        if (page == "assess"):
            recap_token = urllib.parse.parse_qs(parsed_url.query)['q'][0]
            act_string = urllib.parse.parse_qs(parsed_url.query)['a'][0]
            assessment = create_assessment(projectID, recaptchaSiteKey, recap_token, act_string)
            score = assessment.risk_analysis.score
            responseString = str(score)
            if (not assessment.token_properties.valid):
                responseString = str(assessment.token_properties.invalid_reason)
                return http.HTTPStatus.BAD_REQUEST, {}, responseString.encode()
            if (assessment.token_properties.action != act_string):
                responseString = "Expected: "+act_string+"\nActual: "+assessment.token_properties.action+"\n"
                return http.HTTPStatus.BAD_REQUEST, {}, responseString.encode()
            if (act_string == "public" and score > 0.5):
                page = "play.html"
                token = create_token()
            if (act_string == "private" and score > 0.5):
                page = "private.html"
                lobbyKey = create_gamekey()
        if (page in LobbyKeys):
            pstr = page
            page = "play.html"
            token = create_token()
    try:
        p = pathlib.Path(__file__).resolve()
        template = p.parent.joinpath("web").joinpath(page)
    except ValueError:
        pass
    else:
        if template.is_file():
            headers = {"Content-Type": CONTENT_TYPES[template.suffix]}
            body = template.read_bytes()
            if (template.name == 'private.html'):
                body = body.replace(b"KEY_PLACEHOLDER", lobbyKey.encode())
            if (template.name == 'play.html'):
                body = body.replace(b"PSTR_PLACEHOLDER", pstr.encode()).replace(b"TOKEN_PLACEHOLDER", token.encode())
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
        
async def serve_wss(websocket, path):
    try:
        firstMessage = await websocket.recv()
    except websockets.exceptions.ConnectionClosed:
        return
    if (not (firstMessage in Tokens)):
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
    if (not websocket.foundPartner):
        await websocket.close()
        SocketQueue.remove(websocket)
        return
        
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

    
    websocket.gameStatus = "DISCONNECT"
    async for data in websocket:
        await asyncio.sleep(FRAME_DELAY)
        if (isinstance(data, str)):
            if (data == "DISCONNECT" or data == "RESIGN"):
                try:
                    websocket.gameStatus = data
                    await websocket.pairedClient.send(data)
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
                await websocket.send(websocket.pairedClient.gameStatus)
                await websocket.wait_closed()
                return
            except websockets.exceptions.ConnectionClosed:
                return
        except Exception as e:
            await websocket.close()
            await websocket.pairedClient.close()

    if (websocket.close_code == 1001):
        try:
            await websocket.pairedClient.send(websocket.gameStatus)
            await websocket.pairedClient.wait_closed()
        except websockets.exceptions.ConnectionClosed:
            pass

async def main():
    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    htmlSocket = str(pathlib.Path(__file__).resolve().parent.joinpath("web.sock"))
    wssSocket = str(pathlib.Path(__file__).resolve().parent.joinpath("wss.sock"))
    async with websockets.unix_serve(
            noop_handler,
            path=htmlSocket,
            process_request=serve_html
    ), websockets.unix_serve(
        serve_wss,
        path=wssSocket
    ):
        await stop


        
if __name__ == "__main__":
    asyncio.run(main())
