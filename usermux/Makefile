# Makefile for usermux
# 
#   Copyright (C) 1997, 2000, 2012 Free Software Foundation, Inc.
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License as
#   published by the Free Software Foundation; either version 2, or (at
#   your option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

dir := usermux
makemode := server

target = usermux

SRCS = usermux.c mux.c leaf.c node.c stubs.c

OBJS = $(SRCS:.c=.o)
HURDLIBS = netfs fshelp iohelp ports ihash shouldbeinlibc
OTHERLIBS = -lpthread

include ../Makeconf
