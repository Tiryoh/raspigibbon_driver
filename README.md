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


First, download this repository.

```
git clone https://github.com/Tiryoh/raspigibbon_driver.git
```

Next, move into raspigibbon_driver directory and run make command.

```
cd raspigibbon_driver
make && sudo make install
```

## Usage

After installation, test gpio check scripts.  
The LED on the board will blink.

```
./scripts/test_leds.sh
```

## Reference

This device driver is derived from this repository.

[rt-net/RaspberryPiMouse](https://github.com/rt-net/RaspberryPiMouse)

## License

This repository is licensed under the GPLv3 license, see [LICENSE](./LICENSE).

