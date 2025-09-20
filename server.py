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

OUTPUT_BUFFER_FLUSH_INTERVAL = 15
FRAME_DELAY = 0.025
NUMPLAYERS_REFRESH_TIME = 10
MAX_NUMPLAYERS_REFRESHES = 360
TOKEN_LIFETIME = 15
TOKEN_BYTES = 32
TOKEN_LENGTH = TOKEN_BYTES * 2
LOBBY_KEY_LIFETIME = 180
LOBBY_KEY_BYTES = 16
ID_BYTES = 16
GAME_LIFETIME = 1203
STARTUP_TIMEOUT = 300
FRAME_TIMEOUT = 1.5
WEBSOCKET_PING_INTERVAL = 1
MATCHMAKER_SERVICE_SLEEPTIME = 0.01
LOGGER_SERVICE_SLEEPTIME = 0.1
MATCHMAKER_BUFFER_SIZE = 64
LOGGER_BUFFER_SIZE = 64

RECAPTCHA_SITE_KEY = ""
PROJECT_ID = ""
PUBLIC_PAIRSTRING = "public"
TOKEN_COOKIE_NAME = "_PLURABUS_TOKEN_"
ADD_DIRECTIVE = "ADD"
REMOVE_DIRECTIVE = "REMOVE"
END_DIRECTIVE = "END"
ORIGINS = "https://html.itch.zone/"

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
    'buttonClick("private")' : 'document.getElementById("privateform").submit()'
}

#-------------------------Shared resource access------------------------------

class SharedResource:
    def __init__(self, initvar):
        self.var = initvar
        self.lock = trio.Lock()

Tokens = SharedResource({})
Connections = SharedResource({})
HomepageViewerCount = SharedResource(0)
SessionGamesPlayed = SharedResource(0)

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

#--------------------------Matchmaking------------------------------
        
class Matchmaker:
    def __init__(self, num_players):
        self.sendChannel, self.receiveChannel = trio.open_memory_channel(MATCHMAKER_BUFFER_SIZE)
        self.lobbyKeys = SharedResource([])
        self.publicLobbies = []
        self.runningLobbies = []
        self.privateLobbies = {}
        self.num_players = num_players
        
    async def add_player_id(self, identifier):
        async with self.sendChannel.clone() as sender:
            await sender.send(f"{ADD_DIRECTIVE}.{identifier}")

    async def remove_player_id(self, identifier):
        async with self.sendChannel.clone() as sender:
            await sender.send(f"{REMOVE_DIRECTIVE}.{identifier}")

    async def end(self):
        async with self.sendChannel.clone() as sender:
            await sender.send(f"{END_DIRECTIVE}.")
            
    async def create_lobby_key(self):
        lobbyKey = secrets.token_urlsafe(LOBBY_KEY_BYTES)
        async with self.lobbyKeys.lock:
            self.lobbyKeys.var.append(lobbyKey)
        app.nursery.start_soon(remove_shared_later, lobbyKey, self.lobbyKeys, LOBBY_KEY_LIFETIME)
        await MainLogger.log(f"Lobby key created: {lobbyKey}", opt=self)
        return lobbyKey

    async def service_matchmaking_queue(self, nursery):
        while(True):
            directive, identifier = (await self.receiveChannel.receive()).split(".")
            if directive == END_DIRECTIVE:
                await MainLogger.log("Shutting down server")
                nursery.cancel_scope.cancel()
                return
            async with Connections.lock:
                websocket = Connections.var.get(identifier, None)
            if websocket is None or not hasattr(websocket, "pairString"):
                continue
            await MainLogger.log(f"Servicing from queue: {directive} {websocket.remote_addr}", opt=self)
            public = (websocket.pairString == PUBLIC_PAIRSTRING)
            index = 0 if public else websocket.pairString
            lobbies = self.publicLobbies if public else self.privateLobbies
            index_set = range(len(lobbies)) if public else lobbies.keys()
            if directive == ADD_DIRECTIVE:
                if (public and lobbies) or (not public and index in lobbies):
                    lobbies[index].players.append(websocket)
                    websocket.lobby = lobbies[index]
                    await MainLogger.log(f"Added player {websocket.remote_addr}", opt=lobbies[index])
                    if len(lobbies[index].players) == lobbies[index].desiredNumPlayers:
                        startLobby = lobbies.pop(index)
                        startLobby.started = True
                        self.runningLobbies.append(startLobby)
                        nursery.start_soon(startLobby.game)
                else:
                    lobby = Lobby(websocket, self.num_players)
                    await MainLogger.log("Created lobby", opt=lobby)
                    lobbies.append(lobby) if public else lobbies.update({ index : lobby })
            elif directive == REMOVE_DIRECTIVE:
                if hasattr(websocket, "lobby"):
                    if not websocket.lobby.started:
                        websocket.lobby.players.remove(websocket)
                        await MainLogger.log(f"Removed player {websocket.remote_addr}", opt=websocket.lobby)
                        if len(websocket.lobby.players) == 0:
                            try:
                                lobbies.remove(websocket.lobby) if public else lobbies.pop(websocket.pairString)
                                await MainLogger.log("Removed lobby from queue", opt=websocket.lobby)
                            except (KeyError, ValueError):
                                pass
                    else:
                        if websocket.lobby in self.runningLobbies:
                            self.runningLobbies.remove(websocket.lobby)
                async with Connections.lock:
                    Connections.var.pop(identifier)
            await trio.sleep(MATCHMAKER_SERVICE_SLEEPTIME)

#--------------------Main game class--------------------------

class Lobby:
    def __init__(self, websocket, desiredNumPlayers):
        self.players = [websocket]
        self.pairString = websocket.pairString
        self.desiredNumPlayers = desiredNumPlayers
        self.started = False
        websocket.lobby = self

    async def broadcast(self, msg, indexes):
        [await self.players[index].send(msg) for index in indexes]
        
    async def timer_loop(self, parent_scope):
        try:
            for _ in range(GAME_LIFETIME):
                await trio.sleep(1)
                await self.broadcast("TIMER", range(len(self.players)))
            await self.broadcast("TIMEOUT", range(len(self.players)))
        except:
            parent_scope.cancel()
            raise

    async def game_loop(self, parent_scope):
        try:
            while (True):
                await trio.sleep(FRAME_DELAY)
                for index in range(len(self.players)):
                    with trio.move_on_after(FRAME_TIMEOUT) as cancel_scope:
                        msg = await self.players[index].receive()
                    if cancel_scope.cancelled_caught:
                        parent_scope.cancel()
                    await self.broadcast(msg, [i for i in range(len(self.players)) if i != index])                    
        except:
            parent_scope.cancel()
            raise
        
    async def game(self):
        random.shuffle(self.players)
        with trio.move_on_after(FRAME_TIMEOUT) as cancel_scope:
            for playernum in range(len(self.players)):
                websocket = self.players[playernum]
                await websocket.send(self.pairString)
                readymsg = await websocket.receive()
                await websocket.send(f"P{str(playernum + 1)}")
                setmsg = await websocket.receive()
                if (playernum == 0):
                    await websocket.send("Go")
                    startmsg = await websocket.receive()
        if not cancel_scope.cancelled_caught:
            [websocket.gameStarted.set() for websocket in self.players]
            await MainLogger.log("Game started", opt=self)
            async with SessionGamesPlayed.lock:
                SessionGamesPlayed.var += 1
            try:
                async with trio.open_nursery() as nursery:
                    nursery.start_soon(self.timer_loop, nursery.cancel_scope)
                    nursery.start_soon(self.game_loop, nursery.cancel_scope)
            except* Exception as egrp:
                [await MainLogger.log(str(e)) for e in egrp.exceptions]
            finally:
                await MainLogger.log("Game finished", opt=self)
                [websocket.gameFinished.set() for websocket in self.players]

#---------------------Route Handlers-------------------------#

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
    response.headers["Access-Control-Allow-Origin"] = ORIGINS
    response.headers["Cross-Origin-Opener-Policy"] = "same-origin"
    response.headers["Cross-Origin-Embedder-Policy"] = "require-corp"
    response.headers["Cross-Origin-Resource-Policy"] = "cross-origin"
    if not (token is None):
        response.set_cookie(TOKEN_COOKIE_NAME, token, samesite='None', max_age=TOKEN_LIFETIME, secure=True, httponly=True)
    return response

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
    websocket.identifier = secrets.token_hex(ID_BYTES)
    await MainLogger.log("Incoming connection", opt=websocket)
    async with Connections.lock:
        Connections.var.update({ websocket.identifier : websocket })
    try:
        with trio.move_on_after(STARTUP_TIMEOUT) as cancel_scope:
            websocket.pairString = await websocket.receive()
            websocket.gameStarted = trio.Event()
            websocket.gameFinished = trio.Event()
            await MainMatchmaker.add_player_id(websocket.identifier)
            await websocket.gameStarted.wait()
        if not cancel_scope.cancelled_caught:
            await websocket.gameFinished.wait()
    finally:
        with trio.CancelScope(shield = True):
            await MainLogger.log("Ending connection", opt=websocket)
            await MainMatchmaker.remove_player_id(websocket.identifier)

@app.websocket("/d/fourplayergame")
async def serve_fourplayer_game_websocket():
    websocket = quart.websocket._get_current_object()
    websocket.identifier = secrets.token_hex(ID_BYTES)
    await MainLogger.log("Incoming connection", opt=websocket)
    async with Connections.lock:
        Connections.var.update({ websocket.identifier : websocket })
    try:
        with trio.move_on_after(STARTUP_TIMEOUT) as cancel_scope:
            websocket.pairString = await websocket.receive()
            websocket.gameStarted = trio.Event()
            websocket.gameFinished = trio.Event()
            await FourPlayerMatchmaker.add_player_id(websocket.identifier)
            await websocket.gameStarted.wait()
        if not cancel_scope.cancelled_caught:
            await websocket.gameFinished.wait()
    finally:
        with trio.CancelScope(shield = True):
            await MainLogger.log("Ending connection", opt=websocket)
            await FourPlayerMatchmaker.remove_player_id(websocket.identifier)

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

@app.route("/d/public", methods=["POST"])
async def serve_public():
    try:
        postData = await quart.request.get_data()
    except Exception as e:
        await MainLogger.log(f"Bad POST request\n{str(e)}")
        quart.abort(400)
    tok = await create_token(quart.request.remote_addr)
    return await serve_dynamic_file("play.html", { "PSTR_PLACEHOLDER" : PUBLIC_PAIRSTRING, "PMODE_PLACEHOLDER" : "0", "PLAYERS_PLACEHOLDER" : "2" }, token=tok)

@app.route("/d/fourplayer", methods=["POST"])
async def serve_fourplayer():
    try:
        postData = await quart.request.get_data()
    except Exception as e:
        await MainLogger.log(f"Bad POST request\n{str(e)}")
        quart.abort(400)
    tok = await create_token(quart.request.remote_addr)
    return await serve_dynamic_file("play.html", { "PSTR_PLACEHOLDER" : PUBLIC_PAIRSTRING, "PMODE_PLACEHOLDER" : "0", "PLAYERS_PLACEHOLDER" : "4" }, token=tok)


async def serve_private(fourplayer=False): 
    if fourplayer:
        lobbyKey = await FourPlayerMatchmaker.create_lobby_key()
    else:
        lobbyKey = await MainMatchmaker.create_lobby_key()
    return await serve_dynamic_file("private.html", { "KEY_PLACEHOLDER" : lobbyKey })

@app.route("/d/private", methods=["POST"])
async def serve_normalprivate():
    return await serve_private(fourplayer=False)

@app.route("/d/fourplayerprivate", methods=["POST"])
async def serve_fourplayerprivate():
    return await serve_private(fourplayer=True)

@app.route("/d/practice", methods=["POST"])
async def serve_practice():
    try:
        postData = await quart.request.get_data()
    except Exception as e:
        await MainLogger.log(f"Bad POST request\n{str(e)}")
        quart.abort(400)    
    practice_mode = parse_qs(quart.request.query_string.decode("utf-8")).get("m", ["1"])[0]
    tok = await create_token(quart.request.remote_addr)
    return await serve_dynamic_file("play.html", { "PMODE_PLACEHOLDER" : practice_mode, "PLAYERS_PLACEHOLDER" : "2" })
            
@app.route("/g/<string:lobbyKey>", methods=["GET"])
async def serve_http_lobbykey(lobbyKey):
    if len(lobbyKey) > 2 * LOBBY_KEY_BYTES:
        quart.abort(404)
    async with MainMatchmaker.lobbyKeys.lock:
        validKey = lobbyKey in MainMatchmaker.lobbyKeys.var
    if validKey:
        tok = await create_token(quart.request.remote_addr)
        return await serve_dynamic_file("play.html", { "PSTR_PLACEHOLDER" : lobbyKey, "PMODE_PLACEHOLDER" : "0", "PLAYERS_PLACEHOLDER" : "2" }, token=tok)
    async with FourPlayerMatchmaker.lobbyKeys.lock:
        validKey = lobbyKey in FourPlayerMatchmaker.lobbyKeys.var
    if validKey:
        tok = await create_token(quart.request.remote_addr)
        return await serve_dynamic_file("play.html", { "PSTR_PLACEHOLDER" : lobbyKey, "PMODE_PLACEHOLDER" : "0", "PLAYERS_PLACEHOLDER" : "4" }, token=tok)

    else:
        quart.abort(404)

#----------------------Middleware------------------#

class Middleware:
    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        if scope["type"] == "lifespan":
            return await self.app(scope, receive, send)
        if "headers" in scope:
            for header, value in scope["headers"]:
                if header == b"x-real-ip":
                    scope["client"] = (value.decode("utf-8"), None)
        if scope["path"].startswith("/d/game"):
            tokenValid = False
            for header, value in scope["headers"]:
                if header in [b"cookie", b"Cookie", b"COOKIE"]:
                    for cookiename, token in [ kv_pair.strip().split("=", 1) for kv_pair in value.decode("utf-8").split(";") ]:
                        if cookiename == TOKEN_COOKIE_NAME and len(token) == TOKEN_LENGTH:
                            async with Tokens.lock:
                                if (token in Tokens.var):
                                    Tokens.var.pop(token)
                                    tokenValid = True
            if not tokenValid:
                prompt = "no--ip"
                if not (scope["client"] is None):
                    prompt = scope["client"][0]
                await MainLogger.log("Rejected connection, bad token", opt=prompt)
                await send({
                    'type' : 'websocket.http.response.start',
                    'status' : 401,
                    'headers' : [(b'content-length', b'0')],
                })
                await send({
                    'type': 'websocket.http.response.body',
                    'body': b'',
                    'more_body': False,
                })
                return
        return await self.app(scope, receive, send)

#---------------------------Logging----------------------------------#

class Logger:
    def __init__(self):
        self.sendChannel, self.receiveChannel = trio.open_memory_channel(LOGGER_BUFFER_SIZE)

    async def service_log_queue(self):
        while(True):
            line = await self.receiveChannel.receive()
            print(line, flush=True, end="")
            await trio.sleep(LOGGER_SERVICE_SLEEPTIME)

    async def log(self, string, opt=None):
        if opt is None:
            prompt = "Server"
        elif hasattr(opt, "remote_addr"):
            prompt = opt.remote_addr
        elif isinstance(opt, Lobby):
            prompt = f"{opt.pairString[:len(PUBLIC_PAIRSTRING)]}"
            if len(opt.players) > 0:
                prompt += f" {' '.join([player.remote_addr for player in opt.players])}"
        elif isinstance(opt, Matchmaker):
            prompt = "Matchmaker"
        elif isinstance(opt, str):
            prompt = opt
        async with self.sendChannel.clone() as sender:
            await sender.send(f"[{strftime('%x %X')} {prompt}] {string}\n")
    
#-----------------------Main------------------------------------#

MainMatchmaker = Matchmaker(2)
FourPlayerMatchmaker = Matchmaker(4)
MainLogger = Logger()

async def main_app():
    cfg = hypercorn.config.Config()
#    cfg.bind = f"unix:{str(ServerRoot.joinpath('web.sock'))}"
    cfg.bind = "0.0.0.0:5080"
    cfg.workers = 1
    cfg.websocket_ping_interval = WEBSOCKET_PING_INTERVAL
    app.asgi_app = Middleware(app.asgi_app)
    await MainLogger.log("Starting server")
    await hypercorn.trio.serve(app, cfg)

async def main():
    try:
        async with trio.open_nursery() as nursery:
            nursery.start_soon(main_app)
            nursery.start_soon(MainLogger.service_log_queue)
            nursery.start_soon(MainMatchmaker.service_matchmaking_queue, nursery)
            nursery.start_soon(FourPlayerMatchmaker.service_matchmaking_queue, nursery)
    finally:
        with trio.CancelScope(shield=True):
            await MainMatchmaker.end()

if __name__ == "__main__":
    trio_asyncio.run(main)
