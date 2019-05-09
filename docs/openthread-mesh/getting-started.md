# Getting started with OpenThread Mesh Network

This section describes how to quickly run an OpenThread Mesh example without going through the complete toolchain installation. 

There is no need to build any firmware for running it, as it uses the pre-built firmware of [OpenThread CLI](https://github.com/makerdiary/grove-mesh-kit/tree/master/examples/thread/cli) example. This example application demonstrates a minimal OpenThread application that exposes the OpenThread configuration and management interfaces via a basic command-line interface.

## Hardware Requirements

A minimal OpenThread Mesh network requires at least three nodes. Before starting to work, prepare the following parts:

* Grove Mesh Kit for nRF52840-MDK * 3
* Three AA batteries([Energizer® Ultimate Lithium™ AA battery](http://www.energizer.com/batteries/energizer-ultimate-lithium-batteries) is recommended to stay powered longer.)
* A macOS, Linux or Windows computer.

## Assembling the hardware

1. Place the nRF52840-MDK board onto the Base Dock

2. Attach the four plastic spacers supplied in your box to act as legs for the Base Dock

3. Insert an AA battery into the holder the right way around as marked on the board

!!! tip
	AA battery is NOT included in the kit. An [Energizer® Ultimate Lithium™ AA battery](http://www.energizer.com/batteries/energizer-ultimate-lithium-batteries) is recommended to stay powered longer.

## Flashing the OpenThread CLI
Before running the [OpenThread CLI](https://github.com/makerdiary/grove-mesh-kit/tree/master/examples/thread/cli) example, you need to flash the boards. The pre-built firmware is located in `grove-mesh-kit/firmware/openthread/cli`:

<a href="https://github.com/makerdiary/grove-mesh-kit/tree/master/firmware/openthread/cli" target="_blank"><button data-md-color-primary="marsala" style="width: auto;">OpenThread CLI firmware</button></a>

1. Connect one nRF52840-MDK board to your PC using the USB-C cable. A removable drive named **DAPLINK** will appear.

2. Drag and drop the pre-built cli firmware `thread_cli_ftd_uart_nrf52840_mdk.hex` into **DAPLINK**. After flashed, label the board **Node#1** so that later you don't confuse the boards.

3. Program the rest two boards by repeating steps as described above. Label them **Node#2** and **Node#3**

!!! tip
	You can also program the board using [pyOCD](https://github.com/mbedmicro/pyOCD). Just follow this [tutorial](https://wiki.makerdiary.com/nrf52840-mdk/getting-started/#using-pyocd) to set up the pyOCD tool.

## Running the OpenThread CLI

You can access the OpenThread CLI by using a serial terminal like `screen` or [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html). 

### Start Node#1

1. Open a terminal window and run:
	``` sh
	screen /dev/cu.usbmodem141102 115200
	```
	where `/dev/cu.usbmodem141102` is the serial port name of Node#1.

2. press <kbd>Enter</kbd> on the keyboard to bring up the OpenThread CLI `>` prompt.

3. Set the PAN ID:
	``` sh
	panid 0x1234
	```

4. Bring up the IPv6 interface:
	``` sh
	ifconfig up
	```

5. Start Thread protocol operation:
	``` sh
	thread start
	```

6. The LED starts blinking GREEN. Wait a few seconds and verify that the device has become a Thread Leader:
	``` sh
	> state
	leader
	Done
	>
	```

### Start the rest nodes

Start the rest nodes by repeating steps as described above. Wait a minute and verify that the devices have become a Thread Router and a child:

Node#2:
```sh
> state
router
Done
>
```

Node#3:
```sh
> state
child
Done
>
```

### Ping the nodes

View IPv6 addresses assigned to Node#2 and Node#3's Thread interface:

Node#2:
``` sh
> ipaddr
fdde:ad00:beef:0:0:ff:fe00:9c00
fdde:ad00:beef:0:5748:44eb:b417:6d79
fe80:0:0:0:80c:e2b5:510b:8543
Done
>
```

Node#3:
``` sh
> ipaddr
fdde:ad00:beef:0:0:ff:fe00:8000
fdde:ad00:beef:0:d675:a24e:2320:a5a3
fe80:0:0:0:d00a:c9f8:1e0a:dcb0
Done
```

Ping Node#2 and Node#3 from Node#1(the leader):
``` sh
> ping fdde:ad00:beef:0:5748:44eb:b417:6d79
> 16 bytes from fdde:ad00:beef:0:5748:44eb:b417:6d79: icmp_seq=1 hlim=64 time=10ms

> ping fdde:ad00:beef:0:d675:a24e:2320:a5a3
> 16 bytes from fdde:ad00:beef:0:d675:a24e:2320:a5a3: icmp_seq=2 hlim=64 time=39ms

>
```


## Next Steps

Congratulations! You have built a simple OpenThread Mesh network with three nodes. You may use the CLI to change network parameters, other configurations, and perform other operations. See the [OpenThread CLI Reference README.md](https://github.com/makerdiary/openthread/blob/master/src/cli/README.md) to explore more.

See the [Setting Up the Thread SDK](../setting-up-the-thread-sdk) section for information on environment setup. 

Once you set up your Thread development environment, see the [Building & Running the examples](../building-n-running-the-examples) to build the examples on your own machine.

## Create an Issue

Interested in contributing to this project? Want to report a bug? Feel free to click here:

<a href="https://github.com/makerdiary/grove-mesh-kit/issues/new"><button data-md-color-primary="marsala"><i class="fa fa-github"></i> Create an Issue</button></a>


