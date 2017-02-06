#!/bin/bash
#  exit_mn.sh
#
# Copyright (C) 2016-2017  Mays AL-Naday
# All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License version
# 3 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# See LICENSE and COPYING for more details.
#

screen -S mnet -p 0 -X stuff "exit$(printf \\r)"
