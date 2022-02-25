# Chat App
 Chat App in C using TCP Client/Server


Commands to make it work:

gcc Server.c -o Server
gcc Client.c -o Client

./Server
./Client <server_name> 

--------------------------------

<server_name> stands for your own IP Address, in order to find it, type ifconfig -a and copy the IPv4 address
In Client.c, on line 147, replace the ip with your own broadcast ip, this way, the client will be able to send messages to the other clients connected to the server 
