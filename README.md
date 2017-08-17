# VOIP
Real Time Embedded Systems Project (ECEN 5623)

This is an implementation of a one way VOIP system developed using 2 priority preemptive( POSIX SCHED_FIFO) real time services on each end. 


## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. 

### Prerequisites

What things you need to install the software and how to install them

* Both the systems (laptops or any embedded linux platforms) must be running linux. 

* Audio sampling and playback is performed with ALSA (https://www.alsa-project.org/main/index.php/Main_Page). For ALSA installation checkout http://alsa.opensrc.org/Quick_Install.

* Both the machines must be on a LAN. The connection between the two machines will be established through a TCP/IP.


### Installing

A step by step series of examples that tell you have to get a development env running

First git clone the repo on both the machines you want to test.

```
git clone https://github.com/hjjayakrishnan/VOIP.git
```

Then cd into the final_build directory on both machines

```
cd final_build
```
One machine will be running the server and the other the client. Find the IP of the server and change the SERVER_IP #define in both record_server.c and tcp_playback.c.

Then run make

```
make
```
The server machine should then be running record_server.c which will be recording and sending the audio packet. To run type:

```
./record_server
```
The client machine should then be running record_server.c which will be listening to the server. To run type:

```
./tcp_playback
```


## Contributing

The development is still in the initial stages and we are working towards a duplex system. If you would like to contribute please submit a pull request to us.


## Authors

* **Jayakrishnan HJ** - https://github.com/hjjayakrishnan
* **Richard Noronha** - https://github.com/ric1234
* **Sahana Sadagopan** - https://github.com/sahanasadagopan

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

## Acknowledgments

* Sam Siewert - http://www.colorado.edu/ecee/sam-siewert
