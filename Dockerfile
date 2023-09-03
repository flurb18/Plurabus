FROM python:3.10
WORKDIR /
ADD server.py /
ADD web /
RUN pip install trio asyncio trio_asyncio quart quart_trio hypercorn
CMD python server.py --test