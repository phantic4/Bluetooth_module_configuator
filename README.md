# Bluetooth Module Configurator

A Windows C++ GUI for configuring UART Bluetooth modules through a USB-to-TTL adapter.

It supports command presets for:

- HM-10 / BLE UART-style modules
- HC-05
- HC-06
- Generic AT command modules

## Wiring

USB-to-TTL TX goes to module RX.
USB-to-TTL RX goes to module TX.
USB-to-TTL GND goes to module GND.
Use the correct voltage level for your module.

## Build

Open a Developer PowerShell or Developer Command Prompt with CMake available, then run:

```bat
build.bat
```

The app will be built at:

```text
build\Release\bluetooth_module_configurator.exe
```

## Notes

Different clone modules use different AT command sets. The presets cover common HM-10, HC-05, and HC-06 commands, and the raw command box lets you send anything manually.

HM-10 AT mode usually works when the module is not connected over BLE. Many HM-10 firmwares use commands without CR/LF.

HC-05 AT mode normally requires the KEY/EN pin held high during power-up, often at 38400 baud, and commands usually use CR/LF.

HC-06 modules are usually slave-only and often use commands without CR/LF.
