# raspigibbon_driver

device driver of Raspberry Pi Gibbon

## Requirements

* Raspberry Pi 2 or Raspberry Pi 3
  * tested only on Raspberry Pi 3
* Ubuntu Xenial 16.04
  * Ubuntu (4.4.0-1009-raspi2)
  * Ubuntu MATE (4.1.19-v7+)
* linux kernel source
  * download kernel source into `/usr/src/linux`

## Installation

First, check `build-essentials` has been installed.

```
sudo apt install build-essential
```

Next, download this repository.

```
git clone https://github.com/Tiryoh/raspigibbon_driver.git
```

After that, move into raspigibbon_driver directory and run make command.

```
cd raspigibbon_driver
make && sudo make install
```

If you need to uninstall this driver, run make command with uninstall argument.

```
sudo make uninstall
```

## Usage

After installation, test gpio check scripts.

The LED on the board will blink.

```
./scripts/test_leds.sh
```

Status of switches will shown.

```
./scripts/test_switches.sh
```


## References & Includings

This device driver is derived from this repository.

[rt-net/RaspberryPiMouse](https://github.com/rt-net/RaspberryPiMouse)

## License

This repository is licensed under the GPLv3 license, see [LICENSE](./LICENSE).

