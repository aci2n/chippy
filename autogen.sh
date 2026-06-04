#!/bin/sh
# exit on first failing command
set -e
# regenerate configure and Makefile.in; install missing aux scripts (-i)
autoreconf -fi
