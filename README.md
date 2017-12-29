Socket++
========

Socket++ is a modular C++11 warper library for Input/Output systems, especially TCP/IP sockets.

Provides three groups of classes : BaseIO classes, IO protocols, and Connection Handlers :

- BaseIO classes are RAII objects for holding and manipulating underlying ressources, responsible for low level I/O, session/transport management, and addresses. They are responsible for the ressources and must implement some basic I/O methods for reading and writing. Shipped BaseIO classes : BaseFD, BaseSocket, BaseNetSock, BaseSSL, BaseUnixSock, BasePipe, BaseFile
- IO protocols classes are objects used by user for reading/writing. They are the equivalent of OSI's Presentation Layer. These are shipped with socket++ :
  - Simple Socket : Perfect for personal & simple protocols, free of transport problems. Transports different basic things : bools, strings, integers, files, xif::polyvar...
  - Text Socket : Line-oriented, for use of plain old textual protocols like SMTP or HTTP.
  - Tunnel : copy data between two streams
  - ...
- Connection Handlers (or 'ends') are responsible for session-related operations :
  - The socket "server" side, or incoming side : Listen on an address and wait for incoming connections. Manage clients with attached data. Clients can be put in pools waiting for client activity, in autonomous threads, be processed by a callback funtion, or synchronously.
  - The socket "client" side, or outcoming side.

Error/event reporting is based on exceptions. Most objects are reference-counted.