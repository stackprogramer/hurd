/* 
   Copyright (C) 1994 Free Software Foundation

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "priv.h"

/* Unimplemented stubs. */

/* Implement io_readsleep as described in <hurd/io.defs>
   Semantics of ordinary files say this shouldn't happen, because
   we never set use_read_size in the shared data. */
error_t
S_io_readsleep (struct protid *cred)
{
  if (!cred)
    return EOPNOTSUPP;
  
  return 0;
}

/* Implement io_eofnotify as described in <hurd/io.defs>. 
   We don't use this feature. */
error_t
S_io_eofnotify (struct protid *cred)
{
  if (!cred)
    return EOPNOTSUPP;
  
  return 0;
}

/* Implement io_sigio as described in <hurd/io.defs>. 
   We do this (in theory) but async IO is implemented here yet. */
error_t
S_io_sigio (struct protid *cred)
{
  if (!cred)
    return EOPNOTSUPP;
  
  return 0;
}
