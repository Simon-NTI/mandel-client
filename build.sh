#!/bin/sh
`curl-config --cc --cflags` -o a.out main.c `curl-config --libs`
./a.out