# MUDThread

This repository contains the source code for MUDThread, a new security extension to OpenThread. Integrating Manufacturer Usage Description into OpenThread restricts the potential network capabilities of a device running OpenThread. By creating an online available MUD File, MUDThread can create and enforce a local firewall policy for this device.

## Contents

This repository contains two individual systems:

- `ot-br-posix` contains a modified version of the OpenThread Border Router repository used to run a border router on a Raspberry Pi 4B.
- `openthread` contains a modified version of the OpenThread repository used to run end devices on Nordic nRF5340 Development Kits.

## `ot-br-posix`

The OpenThread Border Router is extended with a MUD Manager to download and parse MUD files communicated from end-devices to border routers. This MUD Manager creates an executable script that contains consice `ip6tables` commands to secure a device from unwanted network traffic.

## `openthread`

OpenThread does not contain any MUD processing, and is only extended with a MUD URL (pointing to the MUD File online). This MUD URL is, when enabled, sent to all routers in the Thread network.

This folder contains again two subfolders:

- `cli` is a demo program used to create a command-line interface on a Nordic nRF5340DK System-On-Chip (SOC).
- `src` contains the modified OpenThread Repo taken from the Nordic Connect SDK.
