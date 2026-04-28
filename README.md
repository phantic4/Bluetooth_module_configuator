# Bluetooth Module Configurator

This program lets you change settings on Bluetooth modules with AT commands by using a USB-to-TTL adapter.

You can use it to change things like:

- module name
- baud rate
- role
- PIN if needed

This is useful for HM-10 style modules and other common Bluetooth serial modules.

## What You Need

- a Windows computer
- a USB-TTL adapter or USB-to-TTL cable
- your Bluetooth module
- The TTL-to-Bluetooth-pin Dupont cable if you use one
- this program: `bluetooth_module_configurator.exe`

## Download the EXE

The software can be downloaded from the the [/build/release](https://github.com/phantic4/Bluetooth_module_configuator/blob/main/build/Release/bluetooth_module_configurator.exe?raw=1) folder

## If Windows Blocks the EXE

Do this:

1. Find the EXE file in File Explorer.
2. Right-click the EXE.
3. Click `Properties`.
4. Find the `Unblock` checkbox near the bottom.
5. Check the box.
6. Click `Apply`.
7. Click `OK`.
8. Run the EXE again.

## How to Hook It Up

1. Grab your USB-to-TTL adapter.
2. Plug the USB-TTL adapter into a USB port on the Dell OptiPlex.
3. Grab the TTL-to-Bluetooth-pin Dupont cable.
4. Plug the custom adapter into the USB-to-TTL adapter making sure the ground wire on the TTL-Bluetooth-pin cable is connected to the ground cable on the USB-TTL cable.
5. Plug the Bluetooth module into the other end of teh dupont cable making sure the red wire on the other end is connected to the VCC pin.
7. The module should have a slow blinking red LED and if the module is getting hot unplug all the wires immediately.

## How to Find the COM Port

1. In the search bar, search up Device Manager
2. To down to the ports tab
3. Tlick on the arrow
4. The line where it says something like usb-tll with a COM## next to it

If Device manager is blocked on the computer, follow the steps below


1. Open Arduino IDE.
2. Click `Tools`.
3. Look at `Port` and remember the ports you already see.
4. Unplug the USB-to-TTL adapter.
5. Plug the USB-to-TTL adapter back in.
6. Click `Tools` then `Port` again.
7. The new port that appeared is the one for your USB-to-TTL adapter.
8. Remember that COM port number, like `COM3` or `COM7`.

## How to Use the Program

1. Open `bluetooth_module_configurator.exe`.
2. In the `COM port` box, type the COM port you found in device manager or Arduino IDE, like `COM5`.
3. In the `Connect baud` box, start with `9600` unless your module uses something else.
4. Leave the module preset on the correct module type.
5. Click `Connect`.
6. Look at the message box on the right.
7. If the connection works, you can now send AT commands by using the buttons.

## How to Rename an HM-10 Module

This is the basic way to rename modules one by one.


1. Click in the `New name` box.
2. Type the new name, like `Robot1`.
3. Click `Set Name`.
4. Watch the message box on the right.
5. The program will send the AT command for the rename.
6. If you get an `RX` response showing the change worked, that module is done.


## If It Does Not Work

Try these steps:

1. Make sure the module is plugged in correctly.
2. Make sure the module has power.
3. Make sure the red LED is slowly blinking
3. Make sure you typed the correct COM port.
4. Try `9600` baud first.
5. Make sure the module is not already connected to another device.
6. Try clicking `Test AT` first.
7. Watch the message box for the `RX` response.

## What the Message Box Means

- `TX` means the program sent a command.
- `RX` means the module sent data back.
- If the RX is showing random symbol, then the module might have a problem or the wiring to the module is incorrect
