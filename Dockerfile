FROM emscripten/emsdk:latest AS builder

RUN embuilder.py build sdl2 sdl2_image sdl2_ttf libpng
ADD . /app
WORKDIR /app
RUN mkdir webobj && mkdir web/static/game && make web

FROM python:3.11-slim

WORKDIR /
RUN pip install trio asyncio trio_asyncio quart quart_trio hypercorn
ADD server.py /
COPY --from=builder /app/web /web
CMD python /server.py --test
