#!/usr/bin/env python3

import sys
import minimalmodbus
import serial



def readHoldR(port, slave_address):

    instrument = minimalmodbus.Instrument(port, slave_address)  # port name, slave address (in decimal)
    instrument.serial.baudrate = 115200         # Baud
    instrument.serial.bytesize = 8
    instrument.serial.parity   = serial.PARITY_NONE
    instrument.serial.stopbits = 1
    instrument.serial.timeout  = 0.05          # seconds

    # instrument.write_register(2, value)
    value = instrument.read_register(2)
    print(f" register value: {value}")
    
if __name__ == "__main__":
    if len(sys.argv)<3:
        print(f"usage: {sys.argv[0]} porta indirizzo ")
        exit(1)
    readHoldR(sys.argv[1], int(sys.argv[2]))