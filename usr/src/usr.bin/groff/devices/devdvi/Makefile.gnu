#Copyright (C) 1989, 1990, 1991 Free Software Foundation, Inc.
#     Written by James Clark (jjc@jclark.uucp)
#
#This file is part of groff.
#
#groff is free software; you can redistribute it and/or modify it under
#the terms of the GNU General Public License as published by the Free
#Software Foundation; either version 1, or (at your option) any later
#version.
#
#groff is distributed in the hope that it will be useful, but WITHOUT ANY
#WARRANTY; without even the implied warranty of MERCHANTABILITY or
#FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#for more details.
#
#You should have received a copy of the GNU General Public License along
#with groff; see the file LICENSE.  If not, write to the Free Software
#Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

FONTS=R I B BI CW MI S EX H HI HB
FONTDIR=/usr/local/lib/groff/font
DEVICEDIR=$(FONTDIR)/devdvi

all:

install.nobin:
	-[ -d $(FONTDIR) ] || mkdir $(FONTDIR)
	-[ -d $(DEVICEDIR) ] || mkdir $(DEVICEDIR)
	cp $(FONTS) DESC eqnchar $(DEVICEDIR)

install.bin:

install: install.bin install.nobin

fonts: FORCE
	$(MAKE) -f FontMakefile

FORCE:
