FROM emscripten/emsdk:latest AS builder

RUN embuilder.py build sdl2 sdl2_image sdl2_ttf libpng
ADD ./src /src
ADD ./include /include
ADD ./web /web
ADD ./assets /assets
WORKDIR /
RUN mkdir -p /webobj && mkdir -p /web/static/game && make web

FROM nginx:latest

COPY ./nginx.conf /etc/nginx/nginx.conf

RUN apt update && apt install -y python3 python-is-python3
RUN mkdir /venv && python -m venv /venv
RUN /venv/bin/pip install trio asyncio trio_asyncio quart quart_trio hypercorn
ADD server.py /
COPY --from=builder /web /web
ADD wrapper.sh /
RUN chmod +x /wrapper.sh
CMD bash /wrapper.sh
