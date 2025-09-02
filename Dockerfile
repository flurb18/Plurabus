FROM emscripten/emsdk:latest AS builder

RUN embuilder.py build sdl2 sdl2_image sdl2_ttf libpng sdl2-mt sdl2_image-mt sdl2_ttf-mt libpng-mt
ADD ./src /src
ADD ./include /include
ADD ./assets /assets
ADD ./Makefile /Makefile
WORKDIR /
RUN mkdir /webobj && mkdir /game && make web

FROM nginx:latest

RUN apt update && apt install -y python3 python-is-python3 python3-venv
RUN mkdir /venv && python -m venv /venv
RUN /venv/bin/pip install trio asyncio trio_asyncio quart quart_trio hypercorn
COPY ./nginx.conf /etc/nginx/nginx.conf
ADD server.py /
ADD ./web /web/
COPY --from=builder /game /web/static/game
ADD wrapper.sh /
RUN chmod +x /wrapper.sh
CMD bash /wrapper.sh
