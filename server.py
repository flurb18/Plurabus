#!/usr/bin/env python3

import trio
import asyncio
import trio_asyncio
import quart
import quart_trio
import hypercorn
import json
from time import strftime
from urllib.parse import parse_qs
import argparse
import random
import secrets

parser = argparse.ArgumentParser()
parser.add_argument("--test", help="disable captcha and serve static", action="store_true")
args = parser.parse_args()
if not args.test:
    from google.cloud import recaptchaenterprise_v1

OUTPUT_BUFFER_FLUSH_INTERVAL = 15
FRAME_DELAY = 0.030
NUMPLAYERS_REFRESH_TIME = 10
MAX_NUMPLAYERS_REFRESHES = 360
TOKEN_LIFETIME = 15
TOKEN_BYTES = 32
TOKEN_LENGTH = TOKEN_BYTES * 2
LOBBY_KEY_LIFETIME = 180
LOBBY_KEY_BYTES = 16
ID_BYTES = 24
GAME_LIFETIME = 1203
STARTUP_TIMEOUT = 300
FRAME_TIMEOUT = 5
WEBSOCKET_PING_INTERVAL = 10
MATCHMAKER_SERVICE_SLEEPTIME = 0.01

RECAPTCHA_SITE_KEY = "6LetnQQlAAAAABNjewyT0QnLyxOPkMharK-SILmD"
PROJECT_ID = "skillful-garden-379804"
PUBLIC_PAIRSTRING = "public"
TOKEN_COOKIE_NAME = "PLURABUS_TOKEN"
ADD_DIRECTIVE = "ADD"
REMOVE_DIRECTIVE = "REMOVE"

ServerRoot = trio.Path(__file__).parent
LogFile = ServerRoot.joinpath("logs").joinpath("plurabus.log")

app = quart_trio.QuartTrio(__name__)

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

OutputBuffer = SharedResource([])
Tokens = SharedResource({})
Connections = SharedResource({})
HomepageViewerCount = SharedResource(0)
SessionGamesPlayed = SharedResource(0)

async def in_shared(key, shared):
    async with shared.lock:
        ret = key in shared.var
    return ret

async def remove_shared_later(key, shared, lifetime):
    await trio.sleep(lifetime)
    async with shared.lock:
        if key in shared.var:
            shared.var.remove(key) if isinstance(shared.var, list) else shared.var.pop(key)

async def create_token(remote):
    token = secrets.token_hex(TOKEN_BYTES)
    async with Tokens.lock:
        Tokens.var.update({token : remote })
    app.nursery.start_soon(remove_shared_later, token, Tokens, TOKEN_LIFETIME)
    return token

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

#---------------------------I/O---------------------------------#

async def flush_output_buffer_loop():
    async with await LogFile.open("a") as f:
        while(True):
            await trio.sleep(OUTPUT_BUFFER_FLUSH_INTERVAL)
            async with OutputBuffer.lock:
                await f.writelines(OutputBuffer.var)
                await f.flush()
                OutputBuffer.var.clear()

async def log(string, opt=None):
    if opt is None:
        prompt = "Server"
    elif hasattr(opt, "remote_addr"):
        prompt = opt.remote_addr
    elif isinstance(opt, Lobby):
        prompt = f"{opt.pairString[:len(PUBLIC_PAIRSTRING)]}"
        if len(opt.players) > 0:
            prompt += " "+" ".join([player.remote_addr for player in opt.players])
    elif isinstance(opt, Matchmaker):
        prompt = "Matchmaker"
    async with OutputBuffer.lock:
        OutputBuffer.var.append(f"[{strftime('%x %X')} {prompt}] {string}\n")
        
async def serve_dynamic_file(path, textMap, token=None):
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
    if not (token is None):
        response.set_cookie(TOKEN_COOKIE_NAME, token, samesite='Strict', max_age=TOKEN_LIFETIME, secure=True, httponly=True)
    return response

#--------------------------Matchmaking------------------------------
        
class Matchmaker:
    def __init__(self):
        self.queue = trio.open_memory_channel(0)
        self.lobbyKeys = SharedResource([])
        self.publicLobbies = []
        self.privateLobbies = {}
        
    async def add_player_id(self, identifier):
        async with self.queue[0].clone() as sender:
            await sender.send(f"{ADD_DIRECTIVE}.{identifier}")

    async def remove_player_id(self, identifier):
        async with self.queue[0].clone() as sender:
            await sender.send(f"{REMOVE_DIRECTIVE}.{identifier}")

    async def create_lobby_key(self):
        lobbyKey = secrets.token_urlsafe(LOBBY_KEY_BYTES)
        async with self.lobbyKeys.lock:
            self.lobbyKeys.var.append(lobbyKey)
        app.nursery.start_soon(remove_shared_later, lobbyKey, self.lobbyKeys, LOBBY_KEY_LIFETIME)
        await log(f"Lobby key created: {lobbyKey}", opt=self)
        return lobbyKey

    async def service_queue(self):
        async with trio.open_nursery() as nursery:
            while(True):
                directive, identifier = (await self.queue[1].receive()).split(".")
                async with Connections.lock:
                    websocket = Connections.var.get(identifier, None)
                if websocket is None:
                    continue
                await log(f"Servicing from queue: {directive} {websocket.remote_addr}", opt=self)
                public = (websocket.pairString == PUBLIC_PAIRSTRING)
                index = 0 if public else websocket.pairString
                lobbies = self.publicLobbies if public else self.privateLobbies
                index_set = range(len(lobbies)) if public else lobbies.keys()
                if directive == ADD_DIRECTIVE:
                    if (public and self.publicLobbies) or (not public and websocket.pairString in self.privateLobbies):
                        lobbies[index].players.append(websocket)
                        websocket.lobby = lobbies[index]
                        await log(f"Added player {websocket.remote_addr}", opt=lobbies[index])
                        if len(lobbies[index].players) == lobbies[index].desiredNumPlayers:
                            nursery.start_soon(lobbies.pop(index).game)
                    else:
                        lobby = Lobby(websocket, 2)
                        await log("Created lobby", opt=lobby)
                        lobbies.append(lobby) if public else lobbies.update({ index : lobby })
                elif directive == REMOVE_DIRECTIVE:
                    if hasattr(websocket, "lobby"):
                        websocket.lobby.players.remove(websocket)
                        await log(f"Removed player {websocket.remote_addr}", opt=websocket.lobby)
                        if len(websocket.lobby.players) == 0:
                            try:
                                lobbies.remove(websocket.lobby) if public else lobbies.pop(websocket.pairString)
                                await log("Removed lobby from queue", opt=websocket.lobby)
                            except (KeyError, ValueError):
                                pass
                    async with Connections.lock:
                        Connections.var.pop(identifier)
                await trio.sleep(MATCHMAKER_SERVICE_SLEEPTIME)

#--------------------Main game class--------------------------

class Lobby:
    def __init__(self, websocket, desiredNumPlayers):
        self.players = [websocket]
        self.pairString = websocket.pairString
        self.desiredNumPlayers = desiredNumPlayers
        websocket.lobby = self

    async def broadcast(self, msg, indexes):
        for index in indexes:
            websocket = self.players[index]
            await websocket.send(msg)
        
    async def timer_loop(self, scope):
        for _ in range(GAME_LIFETIME):
            await trio.sleep(1)
            with trio.move_on_after(FRAME_TIMEOUT) as cancel_scope:
                await self.broadcast("TIMER", range(len(self.players)))
            if cancel_scope.cancelled_caught:
                scope.cancel()
                return
        with trio.move_on_after(FRAME_TIMEOUT):
            await self.broadcast("TIMEOUT", range(len(self.players)))

    async def game_loop(self, scope):
        while (True):
            await trio.sleep(FRAME_DELAY)
            for index in range(len(self.players)):
                with trio.move_on_after(FRAME_TIMEOUT) as cancel_scope:
                    msg = await self.players[index].receive()
                    await self.broadcast(msg, [i for i in range(len(self.players)) if i != index])
                if cancel_scope.cancelled_caught:
                    scope.cancel()
                    return

    async def game(self):
        random.shuffle(self.players)
        for playernum in range(len(self.players)):
            websocket = self.players[playernum]
            await websocket.send(self.pairString)
            readymsg = await websocket.receive()
            await websocket.send("P"+str(playernum + 1))
            setmsg = await websocket.receive()
            if (playernum == 0):
                await websocket.send("Go")
                startmsg = await websocket.receive()
            websocket.gameStarted.set()
        await log("Game started", opt=self)
        async with SessionGamesPlayed.lock:
            SessionGamesPlayed.var += 1
        with trio.move_on_after(GAME_LIFETIME+10) as cancel_scope:
            async with trio.open_nursery() as nursery:
                nursery.start_soon(self.timer_loop, cancel_scope)
                nursery.start_soon(self.game_loop, cancel_scope)
        await log("Game finished", opt=self)
        for websocket in self.players:
            websocket.gameFinished.set()
                
#---------------------Route Handlers-------------------------#

@app.websocket("/d/playercount")
async def serve_playercount_websocket():
    async with HomepageViewerCount.lock:
        HomepageViewerCount.var += 1
    try:
        for _ in range(MAX_NUMPLAYERS_REFRESHES):
            async with Connections.lock:
                message = f"Players Online: {str(len(Connections.var))}"
            await quart.websocket.send(message)
            await trio.sleep(NUMPLAYERS_REFRESH_TIME)
    finally:
        with trio.CancelScope(shield = True):
            async with HomepageViewerCount.lock:
                HomepageViewerCount.var -= 1

@app.websocket("/d/game")
async def serve_game_websocket():
    websocket = quart.websocket._get_current_object()
    identifier = secrets.token_hex(ID_BYTES)
    websocket.identifier = identifier
    await log("Incoming connection", opt=websocket)
    async with Connections.lock:
        Connections.var.update({ identifier : websocket })
    try:
        with trio.move_on_after(STARTUP_TIMEOUT) as cancel_scope:
            token = websocket.cookies.get(TOKEN_COOKIE_NAME, "")
            if len(token) != TOKEN_LENGTH:
                await log("Malformed token", opt=websocket)
                cancel_scope.cancel()
            else:
                async with Tokens.lock:
                    tokenValid = token in Tokens.var
                    if tokenValid:
                        tokenValid = tokenValid and (Tokens.var.pop(token) == websocket.remote_addr)
                if not tokenValid:
                    await log("Invalid token", opt=websocket)
                    cancel_scope.cancel()
                else:
                    websocket.pairString = await websocket.receive()
                    websocket.gameStarted = trio.Event()
                    websocket.gameFinished = trio.Event()
                    await MainMatchmaker.add_player_id(identifier)
                    await websocket.gameStarted.wait()
        if not cancel_scope.cancelled_caught:
            await websocket.gameFinished.wait()
    finally:
        with trio.CancelScope(shield = True):
            await log("Ending connection", opt=websocket)
            if not websocket.gameStarted.is_set():
                await MainMatchmaker.remove_player_id(identifier)
            else:
                async with Connections.lock:
                    Connections.var.pop(identifier)

@app.route("/d/serverinfo", methods=["GET"])
async def serve_http_serverinfo():
    info = {}
    async with Connections.lock:
        info.update({"players_online" : len(Connections.var)})
    async with HomepageViewerCount.lock:
        info.update({"on_homepage" : HomepageViewerCount.var})
    async with Tokens.lock:
        info.update({"tokens_active" : len(Tokens.var)})
    async with MainMatchmaker.lobbyKeys.lock:
        info.update({"lobby_keys_active" : len(MainMatchmaker.lobbyKeys.var)})
    async with SessionGamesPlayed.lock:
        info.update({"session_games_played" : SessionGamesPlayed.var})
    response = await quart.make_response(json.dumps(info).encode())
    response.headers["Content-Type"] = ContentTypes[".json"]
    return response
                
@app.route("/d/action", methods=["POST"])
async def serve_http_dynamic():
    actionString = parse_qs(quart.request.query_string.decode("utf-8")).get("a", [""])[0]
    try:
        postData = await quart.request.get_data()
    except Exception as e:
        await log(f"Bad POST request\n{str(e)}")
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
        tok = await create_token(quart.request.remote_addr)
        return await serve_dynamic_file("play.html", { "PSTR_PLACEHOLDER" : PUBLIC_PAIRSTRING }, token=tok)
    elif (actionString == "private"):
        lobbyKey = await MainMatchmaker.create_lobby_key()
        return await serve_dynamic_file("private.html", { "KEY_PLACEHOLDER" : lobbyKey })
    else:
        quart.abort(404)
            
@app.route("/g/<string:lobbyKey>", methods=["GET"])
async def serve_http_lobbykey(lobbyKey):
    if len(lobbyKey) > 2 * LOBBY_KEY_BYTES:
        quart.abort(404)
    if await in_shared(lobbyKey, MainMatchmaker.lobbyKeys):
        tok = await create_token(quart.request.remote_addr)
        return await serve_dynamic_file("play.html", { "PSTR_PLACEHOLDER" : lobbyKey }, token=tok)
    else:
        quart.abort(404)

@app.route("/<path:filePath>", methods=["GET"])
async def serve_static_file(filePath):
    if not args.test:
        quart.abort(404)
    rewrites = NoCaptchaRewrites if filePath in NoCaptchaRewriteFiles else {}
    return await serve_dynamic_file("static/" + filePath, rewrites)

@app.route("/", methods=["GET"])
async def serve_homepage():
    return await serve_static_file("index.html")

#----------------------Accept proxy header middleware-----------#

class ProxyMiddleware:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if "headers" not in scope:
            return await self.app(scope, receive, send)
        for header, value in scope["headers"]:
            if header == b"x-forwarded-for":
                scope["client"] = (value.decode("utf-8"), None)
        return await self.app(scope, receive, send)

#-----------------------Main------------------------------------#

MainMatchmaker = Matchmaker()

async def main_app():
    cfg = hypercorn.config.Config()
    cfg.bind = "unix:"+str(ServerRoot.joinpath("web.sock"))
    cfg.workers = 1
    cfg.websocket_ping_interval = WEBSOCKET_PING_INTERVAL
    app.asgi_app = ProxyMiddleware(app.asgi_app)
    await log("Starting server")
    await hypercorn.trio.serve(app, cfg)

async def main():
    async with trio.open_nursery() as nursery:
        nursery.start_soon(main_app)
        nursery.start_soon(flush_output_buffer_loop)
        nursery.start_soon(MainMatchmaker.service_queue)
    
if __name__ == "__main__":
    trio_asyncio.run(main)
