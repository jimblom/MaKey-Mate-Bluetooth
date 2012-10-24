# MaKey Mate Bluetooth

This is example code for the [MaKey MaKey](https://www.sparkfun.com/products/11511) and [Bluetooth Mate for the MaKey MaKey](https://www.sparkfun.com/products/11378) combo. The goal is to make the combination of both boards feel exactly like the MaKey MaKey...but wireless. Set your banana keyboards and play-doh controllers free!

This example code should be taken as just that... *example* code. It might not work perfectly for everyone's project, but I hope I've made everything it does transparent enough for you to be able to tailor it to your needs.

This code builds on the original [MaKey MaKey code](https://github.com/sparkfun/MaKeyMaKey), written by Jay Silver, Eric Rosenbaum, and I. I've added two files - makeyMate.cpp and makeyMate.h - which handle all of the communication and setup with the RN-42 HID bluetooth module. The main ino file remains mostly unchanged, the major changes were turning any `keyboard.press` and `mouse.move` type commands to `makeyMate` class commands.

## What's Here?

* **maKeyMate_BT** directory - This houses the Arduino example code. Standard to the Arduino file directory structure, the main .ino file shares the same name as the directory.
* **hardware** - This houses the Eagle design files for the schematic and PCB of the Bluetooth Mate for MaKey MaKey.
* [wiki](https://github.com/jimblom/MaKey-Mate-Bluetooth/wiki) - Step-by-step installation guide for the Bluetooth Mate for MaKey MaKey

## License

This project is released as **beerware**. Feel free to use it, with or without attribution, in your own projects. If you find it helpful, buy me a beer (alcoholic or root variety are both fine :) next time we meet up.

## Issues, forking, troubleshooting, and the wiki

If you have any trouble with this code, or are looking for a step-by-step guide, please check out the [wiki](https://github.com/jimblom/MaKey-Mate-Bluetooth/wiki) first. I'll be monitoring the issues, so if the wiki doesn't solve your problem, please post away there.


- Jim Lindblom
SparkFun Electronics
October 24, 2012