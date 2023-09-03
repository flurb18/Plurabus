FROM python:3.11
WORKDIR /
RUN pip install trio asyncio trio_asyncio quart quart_trio hypercorn
ADD server.py /
ADD web /web
CMD python /server.py --test