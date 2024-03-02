FROM emscripten/emsdk:latest AS builder

ADD . /app
WORKDIR /app
RUN make web

FROM python:3.11-slim

WORKDIR /
RUN pip install trio asyncio trio_asyncio quart quart_trio hypercorn
ADD server.py /
COPY --from=builder /app/web /web
CMD python /server.py --test
