Author: Jordan Bartos
Date: February 21, 2020
File: README.md

This file contains instructions for compiling and running ftserver and ftclient programs

# Extra Credit
This implementation of ftserver and ftclient attempts to earn extra credit for the following features:
1. ftclient is multi-threaded. There are separate threads that handle connection P and connection Q
2. The client can change the working directory of the server. The command for this is -c for example, `./ftclient <ftserver host> <ftserver port> <data port> -c <directory_name>` will change the working directory of ftserver to <directory_name> if it is a valid name. If it is invalid, an error message is returned on connection P
3. This implementation of fterver/ftclient can transfer binary as well as text files.


# ftserver
The ftserver program is written in C. It receives requests from ftclient, processes the requests, and attempts to carry out the requested action. If an error is encountered, information about it is returned to ftclient on connection Q. To terminate ftserver, issue a SIGINT to the process with Ctrl+C.

### Compilation instructions and use of makefile
| MAKE COMMAND                 | RESULT                                                         |
| ---------------------------- | -------------------------------------------------------------- |
| `make`                       | compiles ftserver from source code                             |
| `make debug port=<portnum>`  | re-compiles ftserver and runs it with valgrind on port=portnum |
| `make clean`                 | removes executable and .o files for ftserver                   |

### Execution
To run ftserver once it is compiled, issue the command `./ftserver <portnum>` where portnum is the port on which ftserver will listen for incoming connections.


# ftclient
### Compilation
The ftclient program is written as a Python script. If ftclient has execute permissions it can be run directly. If it does not have execute permissions, it must be invoked with an instance of python3.

### Execution
The command to execute ftclient takes one of these two forms:
1. `./ftclient <server_host> <server_port> <client_port> <command> <file_name>`
2.  `python3 ftclient <server_host> <server_port> <client_port> <command> <file_name>`

### Command line arguments
The arguments to ftclient must be supplied in the order specified above. They are:

| ARGUMENT    | DESCRIPTION                                                                                                        |
| ----------- | ------------------------------------------------------------------------------------------------------------------ |
| sever_host  | the hostname of the machine ftserver is running on. This can be the name of the host or the IP address of the host |
| server_port | the port number ftserver is listening for incoming connections on                                                  |
| client_port | the port number on which ftclient should listen for incoming data connections from ftserver                        |
| command     | the command code for the interaction with ftserver                                                                 |
| file_name   | the file or directory name for the interaction with ftserver                                                       |

### Possible commands

| COMMAND | RESULT                                                                                         |
| ------- | ---------------------------------------------------------------------------------------------- |
| `-l`    | list the regular files in ftserver's current working directory                                 |
| `-la`   | list all files and directories (including hidden ones) in ftserver's current working directory |
| `-c`    | change ftserver's current working directory to `<file_name>`                                   |
| `-g`    | get `<file_name>` from ftserver to ftclient                                                    |





