# WinSock
Windows IOCP+线程池的高并发Socket通信框架，带压测用例。4核8线程CPU本地环路测试3秒响应6万个连接请求，每秒近十万条短数据收发

CIOCP类实现完成端口的封装、多线程轮询及事件回调，可用来绑定客户侧socket对象。

