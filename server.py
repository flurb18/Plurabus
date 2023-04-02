#!/usr/bin/env python3

import aiohttp
import aiohttp.web
import aiofiles
import asyncio
import urllib.parse
import pathlib
import uuid
import secrets
import random
from google.cloud import recaptchaenterprise_v1
from google.cloud.recaptchaenterprise_v1 import Assessment

FRAME_DELAY = 0.010
NUMPLAYERS_REFRESH_TIME = 10
MAX_NUMPLAYERS_REFRESHES = 360
TOKEN_LIFETIME = 15
LOBBY_KEY_LIFETIME = 180
LOBBY_KEY_LENGTH = 12
GAME_LIFETIME = 1203

RECAPTCHA_SITE_KEY = "6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD"
PROJECT_ID = "skillful-garden-379804"

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

def create_csp(CSP):
    return "".join("{} {}".format(k,v) for k,v in CSP.items())

def append_to_csp(CSP, key, source):
    CSP[key] = source + " " + CSP[key]

DefaultCSP = {
    "script-src" : "'self' https://www.recaptcha.net/recaptcha/ https://www.gstatic.com/recaptcha/;",
    "img-src" : "'self';",
    "frame-src" : "'self' https://www.recaptcha.net/recaptcha/;",
    "connect-src" : "'self' https://fonts.googleapis.com/ https://fonts.gstatic.com/;",
    "style-src" : "'self' https://fonts.googleapis.com/;",
    "default-src" : "'self' https://fonts.gstatic.com/;",
    "frame-ancestors" : "'self'"
}
DefaultHeaders = {
    "Content-Security-Policy" : create_csp(DefaultCSP),
    "Cross-Origin-Embedder-Policy" : "require-corp",
    "Cross-Origin-Opener-Policy" : "same-origin",
    "Cross-Origin-Resource-Policy" : "same-origin",
    "Strict-Transport-Security" : "max-age=31536000; includeSubDomains",
    "X-Content-Type-Options" : "nosniff",
}
WasmCSP = DefaultCSP.copy()
append_to_csp(WasmCSP, "script-src", "'unsafe-eval'")
WasmHeaders = DefaultHeaders.copy()
WasmHeaders["Content-Security-Policy"] = create_csp(WasmCSP)

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
    lobbyKey = secrets.token_urlsafe(LOBBY_KEY_LENGTH)
    await append_shared(lobbyKey, LobbyKeysLock, LobbyKeys)
    asyncio.ensure_future(remove_shared_later(lobbyKey, LobbyKeysLock, LobbyKeys, LOBBY_KEY_LIFETIME))
    return lobbyKey

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

#--------------------Websocket handlers---------------------------------#

async def serve_playercount_websocket(request):
    try:
        websocket = aiohttp.web.WebSocketResponse()
        await websocket.prepare(request)
        for i in range(MAX_NUMPLAYERS_REFRESHES):
            numplayers = await len_shared(MetadataLock, PlayerMetadata)
            await websocket.send_str("Players Online: " + str(numplayers))
            await asyncio.sleep(NUMPLAYERS_REFRESH_TIME)
    except Exception as e:
        pass
    finally:
        return websocket

async def timerLoop(websocket):
    for seconds in range(GAME_LIFETIME):
        await asyncio.sleep(1)
        try:
            await websocket.send_str("TIMER")
            await websocket.pairedClient.send_str("TIMER")
        except Exception as e:
            return
    try:
        await websocket.send_str("TIMEOUT")
        await websocket.pairedClient.send_str("TIMEOUT")
        await websocket.wait_closed()
        await websocket.pairedClient.wait_closed()
    except Exception as e:
        await websocket.close()
        await websocket.pairedClient.close()
    finally:
        return

async def serve_websocket_wrapper(request):
    request.identifier = uuid.uuid4().hex
    await set_shared(request.identifier, request, MetadataLock, PlayerMetadata)
    try:
        websocket = aiohttp.web.WebSocketResponse()
        await websocket.prepare(request)
        await serve_websocket(websocket)
    except Exception as e:
        print(str(e))
    finally:
        await pop_shared(request.identifier, MetadataLock, PlayerMetadata)
        return websocket

async def serve_websocket(websocket):
    firstMessage = await websocket.receive()
    if (not firstMessage.type == aiohttp.WSMsgType.TEXT):
        return
    token = firstMessage.data
    if (len(token) > 100):
        return
    tokenValid = await check_shared(token, TokenLock, Tokens)
    if (not tokenValid):
        await websocket.close()
        return
    else:
        await remove_shared(token, TokenLock, Tokens)
    secondMessage = await websocket.receive()
    websocket.desiredPairedString = secondMessage.data
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
            ready = await websocket.receive()
        except Exception as e:
            await remove_shared(websocket, SocketLock, SocketQueue)
            return
    else:
        await websocket.send_str(str(websocket.pairedClient.desiredPairedString))
        await websocket.pairedClient.send_str(str(websocket.desiredPairedString))
        ready = await websocket.receive()
            
    if (not websocket.foundPartner):
        await websocket.close()
        await remove_shared(websocket, SocketLock, SocketQueue)
        return
        
    if (websocket.player == 0):
        await websocket.send_str("P1")
    if (websocket.player == 1):
        await websocket.send_str("P2")
    set1 = await websocket.receive()

    websocket.readyForGame.set()
    await websocket.pairedClient.readyForGame.wait()
    
    if (websocket.player == 0):
        await websocket.send_str("Go")
        start = await websocket.receive()
        asyncio.ensure_future(timerLoop(websocket))

    websocket.gameStatus = "DISCONNECT"
    while (True):
        try:
            msg = await websocket.receive()
        except Exception as e:
            await websocket.close()
            await websocket.pairedClient.close()
            return
        await asyncio.sleep(FRAME_DELAY)
        if msg.type == aiohttp.WSMsgType.TEXT:
            if (msg.data == "DISCONNECT" or msg.data == "RESIGN"):
                try:
                    websocket.gameStatus = msg.data
                    await websocket.pairedClient.send_str(msg.data)
                    await websocket.pairedClient.wait_closed()
                    await websocket.wait_closed()
                except Exception as e:
                    await websocket.wait_closed()
                finally:
                    return
            elif (msg.data == "TIMEOUT"):
                await websocket.wait_closed()
                await websocket.pairedClient.wait_closed()
                return
        try:
            await websocket.pairedClient.send_bytes(msg.data)
        except Exception as e:
            await websocket.close()
            await websocket.pairedClient.close()
            return

#---------------------HTTP Handlers-------------------------#
        
async def serve_http_dynamic(request):
    if (request.method != "POST"):
        return aiohttp.web.HTTPNotFound()
    actionString = request.query.get("a", "")
    postData = await request.post()
    recaptchaToken = postData.get("recaptcha-token", "")
    assessment = create_assessment(PROJECT_ID, RECAPTCHA_SITE_KEY, recaptchaToken)
    if (not assessment.token_properties.valid or
        assessment.token_properties.action != actionString or
        assessment.risk_analysis.score < 0.5):
        return aiohttp.web.HTTPUnauthorized(text="Failed captcha")
    if (actionString == "public"):
        token = await create_token()
        return await serve_file_dynamic("play.html", { "TOKEN_PLACEHOLDER" : token, "PSTR_PLACEHOLDER" : "default" }, WasmHeaders.copy())
    elif (actionString == "private"):
        lobbyKey = await create_lobby_key()
        return await serve_file_dynamic("private.html", { "KEY_PLACEHOLDER" : lobbyKey }, DefaultHeaders.copy())
    else:
        return aiohttp.web.HTTPNotFound()

async def serve_http_lobbykey(request):
    key = request.match_info.get("key", "")
    keyValid = await check_shared(key, LobbyKeysLock, LobbyKeys)
    if keyValid:
        token = await create_token()
        return await serve_file_dynamic("play.html", { "TOKEN_PLACEHOLDER" : token, "PSTR_PLACEHOLDER" : key }, WasmHeaders.copy())
    else:
        return aiohttp.web.HTTPNotFound()
        
async def serve_file_dynamic(path, textMap, head):
    try:
        template = ServerRoot.joinpath("web").joinpath(path).resolve()
    except ValueError:
        return aiohttp.web.HTTPNotFound()
    if (not template.is_file()):
        return aiohttp.web.HTTPNotFound()
    async with aiofiles.open(str(template), mode="rb") as f:
        body = await f.read()
    for key in textMap:
        body = body.replace(key.encode(), textMap[key].encode())
    head["Content-Type"] = ContentTypes[template.suffix]
    response = aiohttp.web.Response(body=body, headers=head)
    return response

#-----------------------Main------------------------------------#

def main():
    routes = [
        aiohttp.web.route(method="GET", path="/g/{key:.+}", handler=serve_http_lobbykey),
        aiohttp.web.route(method="POST", path="/d/action", handler=serve_http_dynamic),
        aiohttp.web.route(method="GET", path="/d/websocket", handler=serve_websocket_wrapper),
        aiohttp.web.route(method="GET", path="/d/playercount", handler=serve_playercount_websocket)
    ]
    app = aiohttp.web.Application()
    app.add_routes(routes)
    aiohttp.web.run_app(app, path=str(ServerRoot.joinpath("web.sock")))
    
if __name__ == "__main__":
    main()
