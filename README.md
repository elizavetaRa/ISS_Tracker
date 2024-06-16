# ISS Tracker Application

This is a simple GTK application that uses `osmgpsmap` to display maps and tracks the current location of the ISS using the [Open Notify](http://api.open-notify.org/iss-now.json) API.

## Prerequisites

Before you can compile and run this application, you need to have the following software installed:

1. **GCC (GNU Compiler Collection)**
2. **pkg-config**
3. **GLib 2.0 development files**
4. **GTK+ 3.0 development files**
5. **libosmgpsmap development files**
6. **cURL**
7. **json-c**

## Installation Instructions

### Example: Ubuntu/Debian

You can install all the necessary packages using `apt`:

```sh
sudo apt update
sudo apt install gcc pkg-config libglib2.0-dev libgtk-3-dev libosmgpsmap-1.0-dev libcurl4-openssl-dev libjson-c-dev
```

## Compilation

You can compile the application using the following command:

```sh
gcc -o main main.c $(pkg-config --cflags --libs glib-2.0 gtk+-3.0 osmgpsmap-1.0) -lcurl -ljson-c
```

This will create an executable file that can be opened with

```sh
./main
```
