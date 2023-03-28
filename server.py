#!/usr/bin/env python

import asyncio
import signal
import websockets
import urllib.parse
import http
import pathlib
import uuid
import secrets
import random
from google.cloud import recaptchaenterprise_v1
from google.cloud.recaptchaenterprise_v1 import Assessment

def create_csp(CSP):
    return "".join("{} {}".format(k,v) for k,v in CSP.items())

FRAME_DELAY = 0.010
TOKEN_LIFETIME = 15
LOBBY_KEY_LIFETIME = 180
GAME_LIFETIME = 903

RECAPTCHA_SITE_KEY = "6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD"
PROJECT_ID = "skillful-garden-379804"

CSP_SCRIPT_SRC_WASM = "'self' 'unsafe-eval' https://www.google.com/recaptcha/ https://www.gstatic.com/recaptcha/;"
CSP_IMG_SRC_WASM = "'self' blob:;"
CSP_CONNECT_SRC_WASM = "'self' wss://hivemindga.me/websocket;"

BlockedPrefixes = [ "dyn/" ]
ContentTypes = {
    ".css" : "text/css",
    ".html" : "text/html; charset=utf-8",
    ".txt" : "text/plain",
    ".ico" : "image/x-icon",
    ".png" : "image/png",
    ".js" : "text/javascript",
    ".wasm" : "application/wasm",
    ".data" : "binary"
}
DefaultCSP = {
    "script-src" : "'self' https://www.google.com/recaptcha/ https://www.gstatic.com/recaptcha/;",
    "img-src" : "'self';",
    "frame-src" : "'self' https://www.google.com;",
    "connect-src" : "'self';",
    "default-src" : "'self';"
}
DefaultHeaders = {
    "Content-Security-Policy" : create_csp(DefaultCSP),
    "Cross-Origin-Embedder-Policy" : "require-corp",
    "Cross-Origin-Opener-Policy" : "same-origin",
    "Cross-Origin-Resource-Policy" : "same-origin",
    "Strict-Transport-Security" : "max-age=31536000; includeSubDomains",
    "X-Content-Type-Options" : "nosniff",
    "X-Frame-Options" : "DENY"
}
StatusPages = {
    "not-found" : (http.HTTPStatus.NOT_FOUND, DefaultHeaders, b"Not found\n"),
    "too-long" : (http.HTTPStatus.REQUEST_URI_TOO_LONG, DefaultHeaders, b"Request URI too long\n"),
    "missing-queries" : (http.HTTPStatus.BAD_REQUEST, DefaultHeaders, b"Missing queries\n"),
    "invalid-action" : (http.HTTPStatus.BAD_REQUEST, DefaultHeaders, b"Invalid action\n"),
    "failed-captcha" : (http.HTTPStatus.UNAUTHORIZED, DefaultHeaders, b"Failed captcha\n")
}

Tokens = []
SocketQueue = []
LobbyKeys = []
PlayerMetadata = {}

TokenLock = asyncio.Lock()
SocketLock = asyncio.Lock()
LobbyKeysLock = asyncio.Lock()
MetadataLock = asyncio.Lock()

ServerRoot = pathlib.Path(__file__).parent.resolve()

async def append_shared(element, lock, shared):
    await lock.acquire()
    shared.append(element)
    lock.release()

async def set_shared(key, value, lock, shared):
    await lock.acquire()
    shared[key] = value
    lock.release()

async def get_shared(key, lock, shared):
    await lock.acquire()
    val = shared[key]
    lock.release()
    return val

async def check_shared(element, lock, shared):
    await lock.acquire()
    in_shared = (element in shared)
    lock.release()
    return in_shared

async def len_shared(lock, shared):
    await lock.acquire()
    val = len(shared)
    lock.release()
    return val

async def remove_shared(element, lock, shared):
    await lock.acquire()
    if element in shared:
        shared.remove(element)
    lock.release()

async def remove_shared_later(element, lock, shared, lifetime):
    await asyncio.sleep(lifetime)
    await remove_shared(element, lock, shared)
    
async def pop_shared(key, lock, shared):
    await lock.acquire()
    val = shared.pop(key, None)
    lock.release()
    return val
    
async def create_token():
    token = uuid.uuid4().hex
    await append_shared(token, TokenLock, Tokens)
    asyncio.ensure_future(remove_shared_later(token, TokenLock, Tokens, TOKEN_LIFETIME))
    return token

async def create_lobby_key():
    lobbyKey = secrets.token_urlsafe(12)
    await append_shared(lobbyKey, LobbyKeysLock, LobbyKeys)
    asyncio.ensure_future(remove_shared_later(lobbyKey, LobbyKeysLock, LobbyKeys, LOBBY_KEY_LIFETIME))
    return lobbyKey

# From https://cloud.google.com/recaptcha-enterprise/docs/create-assessment
def create_assessment(project_id, recaptcha_site_key, token):
    client = recaptchaenterprise_v1.RecaptchaEnterpriseServiceClient()
    event = recaptchaenterprise_v1.Event()
    event.site_key = recaptcha_site_key
    event.token = token
    assessment = recaptchaenterprise_v1.Assessment()
    assessment.event = event
    request = recaptchaenterprise_v1.CreateAssessmentRequest()
    request.assessment = assessment
    request.parent = f"projects/{project_id}"
    response = client.create_assessment(request)
    return response

async def serve_html(requrl, request_headers):
    
    parsed_url = urllib.parse.urlparse(requrl)
    pstr = "default"
    token = "default"
    lobbyKey = "default"

    if (parsed_url.path == "" or parsed_url.path == "/"):
        page = "index.html"
    else:
        page = parsed_url.path[1:]
    if (len(page) > 50):
        return StatusPages["too-long"]
    for prefix in BlockedPrefixes:
        if (page.startswith(prefix)):
            return StatusPages["not-found"]
    if (page == "action"):
        queries = urllib.parse.parse_qs(parsed_url.query)
        if (not 'a' in queries or not 't' in queries):
            return StatusPages["missing-queries"]
        if (len(queries['a']) == 0 or len(queries['t']) == 0):
            return StatusPages["missing-queries"]
        actionString = queries['a'][0]
        recaptchaToken = queries['t'][0]
        assessment = create_assessment(PROJECT_ID, RECAPTCHA_SITE_KEY, recaptchaToken)
        if (not assessment.token_properties.valid or
            assessment.token_properties.action != actionString or
            assessment.risk_analysis.score < 0.5):
            return StatusPages["failed-captcha"]
        if (actionString == "public"):
            page = "dyn/play.html"
            token = await create_token()
        elif (actionString == "private"):
            page = "dyn/private.html"
            lobbyKey = await create_lobby_key()
        else:
            return StatusPages["invalid-action"]
    else:
        pageIsLobbyKey = await check_shared(page, LobbyKeysLock, LobbyKeys)
        if (pageIsLobbyKey):
            pstr = page
            page = "dyn/play.html"
            token = await create_token()
    try:
        template = ServerRoot.joinpath("web").joinpath(page).resolve()
    except ValueError:
        return StatusPages["not-found"]
    if (not template.is_file()):
        return StatusPages["not-found"]
    CSP = DefaultCSP.copy()
    body = template.read_bytes()
    if (template.name == "index.html"):
        numplayers = await len_shared(MetadataLock, PlayerMetadata)
        body = body.replace(b"NUMPLAYERS_PLACEHOLDER", ("Players Online: "+str(numplayers)).encode())
    if (template.name == "private.html"):
        body = body.replace(b"KEY_PLACEHOLDER", lobbyKey.encode())
    if (template.name == "play.html"):
        CSP["script-src"] = CSP_SCRIPT_SRC_WASM
        CSP["img-src"] = CSP_IMG_SRC_WASM
        CSP["connect-src"] = CSP_CONNECT_SRC_WASM
        body = body.replace(b"PSTR_PLACEHOLDER", pstr.encode())
        body = body.replace(b"TOKEN_PLACEHOLDER", token.encode())
    headers = DefaultHeaders.copy()
    headers["Content-Type"] = ContentTypes[template.suffix]
    headers["Content-Security-Policy"] = create_csp(CSP)
    return http.HTTPStatus.OK, headers, body

async def serve_websocket(websocket, path):
    try:
        token = await websocket.recv()
    except websockets.exceptions.ConnectionClosed:
        return
    tokenValid = await check_shared(token, TokenLock, Tokens)
    if (not tokenValid):
        await websocket.close(1011, "Invalid token")
        return
    else:
        await remove_shared(token, TokenLock, Tokens)
    websocket.desiredPairedString = await websocket.recv()
    websocket.foundPartner = False
    websocket.readyForGame = asyncio.Event()
    await SocketLock.acquire()
    for waitingClient in SocketQueue:
        if (waitingClient.desiredPairedString == websocket.desiredPairedString):
            SocketQueue.remove(waitingClient)
            websocket.pairedClient = waitingClient
            waitingClient.pairedClient = websocket
            websocket.player = random.randint(0,1)
            waitingClient.player = 1 - websocket.player
            waitingClient.foundPartner = True
            websocket.foundPartner = True
            break
    SocketLock.release()

    if (not websocket.foundPartner):
        await append_shared(websocket, SocketLock, SocketQueue)
        try:
            ready = await websocket.recv()
        except websockets.exceptions.ConnectionClosed:
            await remove_shared(websocket, SocketLock, SocketQueue)
            return
        except Exception as e:
            await remove_shared(websocket, SocketLock, SocketQueue)
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
        await remove_shared(websocket, SocketLock, SocketQueue)
        return
        
    try:
        if (websocket.player == 0):
            await websocket.send("P1")
        if (websocket.player == 1):
            await websocket.send("P2")
        set1 = await websocket.recv()
    except websockets.exceptions.ConnectionClosed:
        await websocket.pairedClient.close()
        return

    websocket.readyForGame.set()
    await websocket.pairedClient.readyForGame.wait()
    
    if (websocket.player == 0):
        try:
            await websocket.send("Go")
            start = await websocket.recv()
        except websockets.exceptions.ConnectionClosed:
            await websocket.pairedClient.close()
            return
        asyncio.ensure_future(timerLoop(websocket))

    websocket.gameStatus = "DISCONNECT"
    while (True):
        try:
            data = await websocket.recv()
        except websockets.exceptions.ConnectionClosed:
            try:
                await websocket.pairedClient.send(websocket.gameStatus)
                await websocket.pairedClient.wait_closed()
            except websockets.exceptions.ConnectionClosed:
                pass
            except Exception as e:
                await websocket.close()
                await websocket.pairedClient.close()
            finally:
                break
#    async for data in websocket:
        await asyncio.sleep(FRAME_DELAY)
        if (isinstance(data, str)):
            if (data == "DISCONNECT" or data == "RESIGN"):
                try:
                    websocket.gameStatus = data
                    await websocket.pairedClient.send(data)
                    await websocket.pairedClient.wait_closed()
                    await websocket.wait_closed()
                except websockets.exceptions.ConnectionClosed:
                    await websocket.wait_closed()
                finally:
                    break
            elif (data == "TIMEOUT"):
                await websocket.wait_closed()
                await websocket.pairedClient.wait_closed()
                return
        try:
            await websocket.pairedClient.send(data)
        except websockets.exceptions.ConnectionClosed:
            try:
                await websocket.send(websocket.pairedClient.gameStatus)
                await websocket.wait_closed()
            except websockets.exceptions.ConnectionClosed:
                pass
            except Exception as e:
                await websocket.close()
                await websocket.pairedClient.close()
            finally:
                break
        except Exception as e:
            await websocket.close()
            await websocket.pairedClient.close()
            break

    if (websocket.close_code == 1001):
        try:
            await websocket.pairedClient.send(websocket.gameStatus)
            await websocket.pairedClient.wait_closed()
        except websockets.exceptions.ConnectionClosed:
            pass

async def timerLoop(websocket):
    for seconds in range(GAME_LIFETIME):
        await asyncio.sleep(1)
        try:
            await websocket.send("TIMER")
            await websocket.pairedClient.send("TIMER")
        except websockets.exceptions.ConnectionClosed:
            return
        except Exception as e:
            return
    try:
        await websocket.send("TIMEOUT")
        await websocket.pairedClient.send("TIMEOUT")
        await websocket.wait_closed()
        await websocket.pairedClient.wait_closed()
    except websockets.exceptions.ConnectionClosed:
        pass
    except Exception as e:
        await websocket.close()
        await websocket.pairedClient.close()
    finally:
        return

async def serve_websocket_wrapper(websocket, path):
    websocket.identifier = uuid.uuid4().hex
    await set_shared(websocket.identifier, websocket, MetadataLock, PlayerMetadata)
    try:
        await serve_websocket(websocket, path)
    except Exception as e:
        print(str(e))
    finally:
        await pop_shared(websocket.identifier, MetadataLock, PlayerMetadata)

async def serve_nothing(websocket, path):
    pass

async def main():
    loop = asyncio.get_running_loop()
    stop = loop.create_future()
    loop.add_signal_handler(signal.SIGTERM, stop.set_result, None)
    htmlSocket = str(ServerRoot.joinpath("web.sock"))
    wssSocket = str(ServerRoot.joinpath("wss.sock"))
    async with websockets.unix_serve(
            serve_websocket_wrapper, path=wssSocket
    ), websockets.unix_serve(
        serve_nothing, path=htmlSocket, process_request=serve_html
    ):
        await stop
        
if __name__ == "__main__":
    asyncio.run(main())
