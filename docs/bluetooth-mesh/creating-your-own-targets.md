# Creating your own targets

This section describes how to create a new build target for your own mesh application.

## Creating a new build target

The easiest way to create a new build target is to:

1. Copy one of the example folders in `nRF5-SDK-for-Mesh`, e.g., `examples/my_beaconing` to `examples/my_app`.

2. Add the folder to the `examples/CMakeLists.txt` with a `add_subdirectory("my_app")` command.

3. Modify the target name in the first line of `examples/my_app/CMakeLists.txt` to `set(target "my_app")`.

4. Open terminal and change directory to:

	``` sh
	cd ./grove-mesh-kit
	```

5. Creat a `build` folder and change directory to it:
	``` sh
	mkdir build && cd build
	```

6. Generating build files with
	``` sh
	cmake -G Ninja -DTOOLCHAIN=gccarmemb -DPLATFORM=nrf52840_xxAA -DBOARD=nrf52840_mdk -DFLASHER=pyocd ../nrf_sdks/nRF5-SDK-for-Mesh/
	```

7. Build your new target with:
	``` sh
	ninja my_app
	```

8. Flash the target:
	``` sh
	ninja flash_my_app
	```

## API Reference

Check out the [Mesh API Reference](https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.meshsdk.v3.1.0/modules.html) for resources.

## Create an Issue

Interested in contributing to this project? Want to report a bug? Feel free to click here:

<a href="https://github.com/makerdiary/grove-mesh-kit/issues/new"><button data-md-color-primary="marsala"><i class="fa fa-github"></i> Create an Issue</button></a>


