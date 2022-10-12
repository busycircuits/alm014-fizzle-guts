#!/bin/sh
avrdude -V -p atmega644p -P usb -c usbtiny -F -e -U hfuse:w:0xD8:m -Ulfuse:w:0xFF:m -U efuse:w:0x04:m -U flash:w:fz1.hex:i

