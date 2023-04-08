#!/usr/bin/env python3

import trio
import warnings

warnings.filterwarnings(action="ignore", category=trio.TrioDeprecationWarning)

import asyncio
import trio_asyncio
import quart
import quart_trio
import hypercorn
from wsproto.utilities import LocalProtocolError
import json
from urllib.parse import parse_qs
import argparse
import pathlib
import uuid
import secrets
import random

parser = argparse.ArgumentParser()
parser.add_argument("--test", help="disable captcha and serve static", action="store_true")
args = parser.parse_args()
if not args.test:
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

ServerRoot = trio.Path(str(pathlib.Path(__file__).parent.resolve()))

app = quart_trio.QuartTrio(__name__)

cfg = hypercorn.config.Config()
cfg.bind = "unix:"+str(ServerRoot.joinpath("web.sock"))
cfg.workers = 1
cfg.websocket_ping_interval = 5.0

#----------------------Header Definitions---------------------------

ContentTypes = {
    ".css" : "text/css",
    ".html" : "text/html; charset=utf-8",
    ".txt" : "text/plain",
    ".ico" : "image/x-icon",
    ".png" : "image/png",
    ".js" : "text/javascript",
    ".json" : "application/json",
    ".wasm" : "application/wasm"
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

#---------------Definitions for captcha-disabled version---------------------

NoCaptchaRewriteFiles = [
    "script/index.js",
    "index.html"
]
NoCaptchaRewrites = {
    'buttonClick("public")' : 'document.getElementById("publicform").submit()',
    'buttonClick("private")' : 'document.getElementById("privateform").submit()',
    '<script src="https://www.recaptcha.net/recaptcha/enterprise.js?render=6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD"></script>' : ''
}

#-------------------------Shared resource access------------------------------

class SharedResource:
    def __init__(self, initvar):
        self.var = initvar
        self.lock = trio.Lock()

Tokens = SharedResource([])
LobbyKeys = SharedResource([])
PublicQueue = SharedResource([])
PrivateGames = SharedResource({})
PlayerCount = SharedResource(0)
HomepageViewerCount = SharedResource(0)
SessionGamesPlayed = SharedResource(0)

async def remove_shared_later(element, shared, lifetime):
    await trio.sleep(lifetime)
    async with shared.lock:
        if element in shared.var:
            shared.var.remove(element)

async def create_token():
    token = uuid.uuid4().hex
    async with Tokens.lock:
        Tokens.var.append(token)
    app.nursery.start_soon(remove_shared_later, token, Tokens, TOKEN_LIFETIME)
    return token

async def create_lobby_key():
    lobbyKey = secrets.token_urlsafe(LOBBY_KEY_BYTES)
    async with LobbyKeys.lock:
        LobbyKeys.var.append(lobbyKey)
    app.nursery.start_soon(remove_shared_later, lobbyKey, LobbyKeys, LOBBY_KEY_LIFETIME)
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
    try:
        template = await ServerRoot.joinpath("web").joinpath(path).resolve()
    except ValueError:
        quart.abort(404)
    if not await template.is_file():
        quart.abort(404)
    async with await template.open("rb") as f:
        body = await f.read()
    for key, value in textMap.items():
        body = body.replace(key.encode(), value.encode())
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
        async with Tokens.lock:
            tokenValid = firstMessage in Tokens.var
            if tokenValid:
                Tokens.var.remove(firstMessage)
        if not tokenValid:
            return
        pairString = await websocket.receive()
        websocket.foundPartner = False
        websocket.readyForGame = trio.Event()
        public = (pairString == PUBLIC_PAIRSTRING)
        lock = PublicQueue.lock if public else PrivateGames.lock
        async with lock:
            if (public and PublicQueue.var) or (not public and pairString in PrivateGames.var):
                partner = PublicQueue.var.pop(0) if public else PrivateGames.var.pop(pairString)
                websocket.pairedClient = partner
                partner.pairedClient = websocket
                websocket.player = random.randint(0,1)
                partner.player = 1 - websocket.player
                websocket.foundPartner = True
                partner.foundPartner = True
            else:
                PublicQueue.var.append(websocket) if public else PrivateGames.var.update({pairString : websocket})
        if websocket.foundPartner:
            await websocket.send(pairString)
            await websocket.pairedClient.send(pairString)
        try:
            readymsg = await websocket.receive()
        finally:
            with trio.CancelScope(shield = True):
                if not websocket.foundPartner:
                    async with lock:
                        PublicQueue.var.remove(websocket) if public else PrivateGames.var.pop(pairString)
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
                async with SessionGamesPlayed.lock:
                    SessionGamesPlayed.var += 1
                nursery.start_soon(timer_loop, websocket)

    
#---------------------Route Handlers-------------------------#

@app.websocket("/d/playercount")
async def serve_playercount_websocket():
    async with HomepageViewerCount.lock:
        HomepageViewerCount.var += 1
    try:
        for _ in range(MAX_NUMPLAYERS_REFRESHES):
            async with PlayerCount.lock:
                message = "Players Online: " + str(PlayerCount.var)
            await quart.websocket.send(message)
            await trio.sleep(NUMPLAYERS_REFRESH_TIME)
    except* LocalProtocolError:
        pass
    finally:
        with trio.CancelScope(shield = True):
            async with HomepageViewerCount.lock:
                HomepageViewerCount.var -= 1

@app.websocket("/d/game")
async def serve_game_websocket_wrapper():
    async with PlayerCount.lock:
        PlayerCount.var += 1
    try:
        await serve_game_websocket(quart.websocket._get_current_object())
    except* LocalProtocolError:
        pass
    finally:
        with trio.CancelScope(shield = True):
            async with PlayerCount.lock:
                PlayerCount.var -= 1

@app.route("/d/serverinfo", methods=["GET"])
async def serve_http_serverinfo():
    info = {}
    async with PlayerCount.lock:
        info.update({"players_online" : str(PlayerCount.var)})
    async with HomepageViewerCount.lock:
        info.update({"on_homepage" : str(HomepageViewerCount.var)})
    async with Tokens.lock:
        info.update({"tokens_active" : str(len(Tokens.var))})
    async with LobbyKeys.lock:
        info.update({"lobby_keys_active" : str(len(LobbyKeys.var))})
    async with PublicQueue.lock:
        info.update({"queue_size" : str(len(PublicQueue.var))})
    async with PrivateGames.lock:
        info.update({"private_games_waiting" : str(len(PrivateGames.var))})
    async with SessionGamesPlayed.lock:
        info.update({"session_games_played" : str(SessionGamesPlayed.var)})
    response = await quart.make_response(json.dumps(info).encode())
    response.headers["Content-Type"] = ContentTypes[".json"]
    response.headers["Content-Security-Policy"] = DefaultCSP
    return response
                
@app.route("/d/action", methods=["POST"])
async def serve_http_dynamic():
    actionString = parse_qs(quart.request.query_string.decode("utf-8")).get("a", [""])[0]
    try:
        postData = await quart.request.get_data()
    except Exception as e:
        quart.abort(400)
    recaptchaToken = parse_qs(postData.decode("utf-8")).get("recaptcha-token",[""])[0]
    if not args.test:
        if actionString == "" or recaptchaToken == "":
            quart.abort(400)
        assessment = await create_assessment(PROJECT_ID, RECAPTCHA_SITE_KEY, recaptchaToken)
        if (not assessment.token_properties.valid or
            assessment.token_properties.action != actionString or
            assessment.risk_analysis.score < 0.5):
            quart.abort(401, "Failed captcha")
    if (actionString == "public"):
        token = await create_token()
        return await serve_dynamic_file("play.html",{ "TOKEN_PLACEHOLDER" : token, "PSTR_PLACEHOLDER" : PUBLIC_PAIRSTRING })
    elif (actionString == "private"):
        lobbyKey = await create_lobby_key()
        return await serve_dynamic_file("private.html",{ "KEY_PLACEHOLDER" : lobbyKey }, wasm = False)
    else:
        quart.abort(404)
            
@app.route("/g/<string:lobbyKey>", methods=["GET"])
async def serve_http_lobbykey(lobbyKey):
    if len(lobbyKey) > 32:
        quart.abort(404)
    async with LobbyKeys.lock:
        keyValid = lobbyKey in LobbyKeys.var
    if keyValid:
        token = await create_token()
        return await serve_dynamic_file("play.html",{ "TOKEN_PLACEHOLDER" : token, "PSTR_PLACEHOLDER" : lobbyKey })
    else:
        quart.abort(404)

@app.route("/<path:filePath>", methods=["GET"])
async def serve_static_file(filePath):
    if not args.test:
        quart.abort(404)
    rewrites = NoCaptchaRewrites if filePath in NoCaptchaRewriteFiles else {}
    return await serve_dynamic_file("static/" + filePath, rewrites, wasm = False)

@app.route("/", methods=["GET"])
async def serve_homepage():
    return await serve_static_file("index.html")
    
#-----------------------Main------------------------------------#

async def main():
    await hypercorn.trio.serve(app, cfg)
    
if __name__ == "__main__":
    trio_asyncio.run(main)
