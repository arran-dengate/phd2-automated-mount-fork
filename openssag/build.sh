#!/bin/bash

sudo ./autogen.sh
sudo ./configure
sudo make
sudo make install
cd src
g++ main.cpp -lusb -lopenssag
cd ..
