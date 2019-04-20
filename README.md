# WinSock库介绍
Windows IOCP+线程池的高并发Socket通信框架。

* CIOCP类实现完成端口的封装、多线程轮询及事件回调，可用来绑定客户侧socket对象。

* CClientSock为客户侧socket，提供连接服务端、检查连接状态、异步接收数据的方法，可绑定到windows线程池或iocp对象。

* CServerSock为服务端socket，提供非阻塞接受连接的方法，内置iocp对象实现实时监听和数据收发，并提供接收、断开的事件回调。
可在服务端接受连接的回调中调用keepAlive方法开启保活机制。

main.cpp中有压测用例，4核8线程CPU本地环路测试，3秒响应6万个连接请求，每秒近十万条短数据收发。
