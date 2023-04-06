#!/usr/bin/env python3

import trio
import asyncio
import trio_asyncio
import quart
import quart_trio
import hypercorn
from urllib.parse import parse_qs
import pathlib
import uuid
import secrets
import random
from google.cloud import recaptchaenterprise_v1

FRAME_DELAY = 0.010
NUMPLAYERS_REFRESH_TIME = 10
MAX_NUMPLAYERS_REFRESHES = 360
TOKEN_LIFETIME = 15
TOKEN_LENGTH = 32
LOBBY_KEY_LIFETIME = 180
LOBBY_KEY_BYTES = 12
GAME_LIFETIME = 1203
STARTUP_TIMEOUT = 300
FRAME_TIMEOUT = 5

RECAPTCHA_SITE_KEY = "6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD"
PROJECT_ID = "skillful-garden-379804"
PUBLIC_PAIRSTRING = "default"

ServerRoot = pathlib.Path(__file__).parent.resolve()

app = quart_trio.QuartTrio(__name__)

cfg = hypercorn.config.Config()
cfg.bind = "unix:"+str(ServerRoot.joinpath("web.sock"))
cfg.workers = 1

#----------------------Header Definitions---------------------------

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
CSPDict = {
    "script-src" : "'self' https://www.recaptcha.net/recaptcha/ https://www.gstatic.com/recaptcha/;",
    "img-src" : "'self';",
    "frame-src" : "'self' https://www.recaptcha.net/recaptcha/;",
    "connect-src" : "'self' https://fonts.googleapis.com/ https://fonts.gstatic.com/;",
    "style-src" : "'self' https://fonts.googleapis.com/;",
    "default-src" : "'self' https://fonts.gstatic.com/;",
    "frame-ancestors" : "'self';"
}
WasmCSPDict = CSPDict.copy()
WasmCSPDict["script-src"] = "'unsafe-eval' " + CSPDict["script-src"]

def create_csp(CSP):
    return " ".join("{} {}".format(k,v) for k,v in CSP.items())

DefaultCSP = create_csp(CSPDict)
WasmCSP = create_csp(WasmCSPDict)

#-------------------------Shared resource access------------------------------

Tokens = []
LobbyKeys = []
PublicQueue = []
PrivateGames = {}
PlayerCount = 0

TokenLock = trio.Lock()
LobbyKeysLock = trio.Lock()
PublicLock = trio.Lock()
PrivateLock = trio.Lock()
CountLock = trio.Lock()

async def add_player():
    global PlayerCount
    async with CountLock:
        PlayerCount += 1

async def remove_player():
    global PlayerCount
    async with CountLock:
        PlayerCount -= 1

async def append_shared(element, lock, shared):
    async with lock:
        shared.append(element)

async def check_shared(element, lock, shared):
    async with lock:
        in_shared = (element in shared)
    return in_shared

async def len_shared(lock, shared):
    async with lock:
        val = len(shared)
    return val

async def remove_shared(element, lock, shared):
    async with lock:
        if element in shared:
            shared.remove(element)

async def remove_shared_later(element, lock, shared, lifetime):
    await trio.sleep(lifetime)
    await remove_shared(element, lock, shared)
    
async def create_token():
    token = uuid.uuid4().hex
    await append_shared(token, TokenLock, Tokens)
    app.nursery.start_soon(remove_shared_later, token, TokenLock, Tokens, TOKEN_LIFETIME)
    return token

async def create_lobby_key():
    lobbyKey = secrets.token_urlsafe(LOBBY_KEY_BYTES)
    await append_shared(lobbyKey, LobbyKeysLock, LobbyKeys)
    app.nursery.start_soon(remove_shared_later, lobbyKey, LobbyKeysLock, LobbyKeys, LOBBY_KEY_LIFETIME)
    return lobbyKey

#---------------------------------Captcha--------------------------------------

async def create_assessment(project_id, recaptcha_site_key, token):
    client = recaptchaenterprise_v1.RecaptchaEnterpriseServiceAsyncClient()
    event = recaptchaenterprise_v1.Event()
    event.site_key = recaptcha_site_key
    event.token = token
    assessment = recaptchaenterprise_v1.Assessment()
    assessment.event = event
    request = recaptchaenterprise_v1.CreateAssessmentRequest()
    request.assessment = assessment
    request.parent = f"projects/{project_id}"
    response = await trio_asyncio.aio_as_trio(client.create_assessment)(request)
    return response

#-----------------------Dynamic file responder-----------------------

async def serve_dynamic_file(path, textMap, wasm = True):
    template = ServerRoot.joinpath("web").joinpath(path).resolve()
    async with await trio.Path(str(template)).open("rb") as f:
        body = await f.read()
    for key in textMap:
        body = body.replace(key.encode(), textMap[key].encode())
    response = await quart.make_response(body)
    response.headers["Content-Type"] = ContentTypes[template.suffix]
    response.headers["Content-Security-Policy"] = WasmCSP if wasm else DefaultCSP
    return response

#--------------------Main game Websocket methods--------------------
    
async def timer_loop(websocket):
    for _ in range(GAME_LIFETIME):
        await trio.sleep(1)
        await websocket.send("TIMER")
        await websocket.pairedClient.send("TIMER")
    await websocket.send("TIMEOUT")
    await websocket.pairedClient.send("TIMEOUT")

async def game_loop(websocket):
    while (True):
        with trio.move_on_after(FRAME_TIMEOUT) as cancel_scope:
            await trio.sleep(FRAME_DELAY)
            msg = await websocket.receive()
            await websocket.pairedClient.send(msg)
        if cancel_scope.cancelled_caught:
            await websocket.send("FRAME_TIMEOUT")
            return
    
async def serve_game_websocket(websocket):
    with trio.move_on_after(STARTUP_TIMEOUT) as cancel_scope:
        firstMessage = await websocket.receive()
        if not isinstance(firstMessage, str):
            return
        if len(firstMessage) != TOKEN_LENGTH:
            return
        tokenValid = await check_shared(firstMessage, TokenLock, Tokens)
        if not tokenValid:
            return
        await remove_shared(firstMessage, TokenLock, Tokens)
        pairString = await websocket.receive()
        websocket.foundPartner = False
        websocket.readyForGame = trio.Event()
        public = (pairString == PUBLIC_PAIRSTRING)
        async with (PublicLock if public else PrivateLock):
            if (public and PublicQueue) or (not public and pairString in PrivateGames):
                partner = PublicQueue.pop(0) if public else PrivateGames.pop(pairString)
                websocket.pairedClient = partner
                partner.pairedClient = websocket
                websocket.player = random.randint(0,1)
                partner.player = 1 - websocket.player
                websocket.foundPartner = True
                partner.foundPartner = True
            else:
                PublicQueue.append(websocket) if public else PrivateGames.update({pairString : websocket})
        if websocket.foundPartner:
            await websocket.send(pairString)
            await websocket.pairedClient.send(pairString)
        try:
            readymsg = await websocket.receive()
        finally:
            with trio.CancelScope(shield = True):
                if not websocket.foundPartner:
                    await remove_shared(websocket, PublicLock, PublicQueue)
                    return

        await websocket.send("P"+str(websocket.player + 1))
        setmsg = await websocket.receive()

        websocket.readyForGame.set()
        await websocket.pairedClient.readyForGame.wait()
    
        if (websocket.player == 0):
            await websocket.send("Go")
            startmsg = await websocket.receive()

    if not cancel_scope.cancelled_caught:
        async with trio.open_nursery() as nursery:
            nursery.start_soon(game_loop, websocket)
            if (websocket.player == 0):
                nursery.start_soon(timer_loop, websocket)

    
#---------------------Route Handlers-------------------------#

@app.websocket("/d/playercount")
async def serve_playercount_websocket():
    for _ in range(MAX_NUMPLAYERS_REFRESHES):
        async with CountLock:
            message = "Players Online: " + str(PlayerCount)
        await quart.websocket.send(message)
        await trio.sleep(NUMPLAYERS_REFRESH_TIME)

@app.websocket("/d/game")
async def serve_game_websocket_wrapper():
    await add_player()
    try:
        await serve_game_websocket(quart.websocket._get_current_object())
    finally:
        with trio.CancelScope(shield = True):
            await remove_player()

@app.route("/d/action", methods=["POST"])
async def serve_http_dynamic():
    actionString = parse_qs(quart.request.query_string.decode("utf-8")).get("a", [""])[0]
    try:
        postData = await quart.request.get_data()
    except Exception as e:
        quart.abort(400)
    recaptchaToken = parse_qs(postData.decode("utf-8")).get("recaptcha-token",[""])[0]
    if actionString == "" or recaptchaToken == "":
        quart.abort(400)
    assessment = await create_assessment(PROJECT_ID, RECAPTCHA_SITE_KEY, recaptchaToken)
    if (not assessment.token_properties.valid or
        assessment.token_properties.action != actionString or
        assessment.risk_analysis.score < 0.5):
        quart.abort(401, "Failed captcha")
    else:
        if (actionString == "public"):
            token = await create_token()
            return await serve_dynamic_file(
                "play.html",
                { "TOKEN_PLACEHOLDER" : token, "PSTR_PLACEHOLDER" : PUBLIC_PAIRSTRING }
            )
        elif (actionString == "private"):
            lobbyKey = await create_lobby_key()
            return await serve_dynamic_file(
                "private.html",
                { "KEY_PLACEHOLDER" : lobbyKey },
                wasm = False
            )
        else:
            quart.abort(404)

@app.route("/g/<string:lobbyKey>")
async def serve_http_lobbykey(lobbyKey):
    if len(lobbyKey) > 32:
        quart.abort(404)
    keyValid = await check_shared(lobbyKey, LobbyKeysLock, LobbyKeys)
    if keyValid:
        token = await create_token()
        return await serve_dynamic_file(
            "play.html",
            { "TOKEN_PLACEHOLDER" : token, "PSTR_PLACEHOLDER" : lobbyKey }
        )
    else:
        quart.abort(404)
        
#-----------------------Main------------------------------------#

async def main():
    await hypercorn.trio.serve(app, cfg)
    
if __name__ == "__main__":
    trio_asyncio.run(main)
