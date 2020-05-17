# A Simple C Web Server

A simple http web server implemented in C, without any web frameworks. Hosts the contents of Santa Clara University's [home page](https://www.scu.edu) (as of 4/28/20). Only supports HTTP/1.0 and GET requests. Utilizes an event-driven architecture rather than multithreading for handling concurrent requests.

## Build
`make`

## Build with debugging
`make DEBUG=1`

## Run
`./server -port <port> -document_root <document_root>`