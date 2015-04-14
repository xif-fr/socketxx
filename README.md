socket++
========

Socket++ is a modular C++ warper library for Input/Output systems, especially TCP/IP sockets.
It furnishes three groups of classes :
	* BaseIO classes : RAII objects for holding and manipulating underlying ressources, responsible for low level I/O, session/transport management, and addresses.
	* IO protocol classes : Implement a generic familly of protocols (OSI's Presentation Layer). High level final input/output and communication interface.
	* Connection Handlers, or 'ends' : Responsible for session-related operations and opening/connecting, like client or server side classes