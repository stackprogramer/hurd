/* Make a new trivfs peropen/protid

   Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   The GNU Hurd is distributed in the hope that it will be useful, 
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <string.h>		/* For bcopy() */

#include "priv.h"

/* Return a new protid pointing to a new peropen in CRED, with REALNODE as
   the underlying node reference, with the given identity, and open flags in
   FLAGS.  CNTL is the trivfs control object.  */
error_t
trivfs_open (struct trivfs_control *cntl,
	     uid_t *uids, unsigned num_uids, gid_t *gids, unsigned num_gids,
	     unsigned flags,
	     mach_port_t realnode,
	     struct trivfs_protid **cred)
{
  error_t err = 0;
  struct trivfs_peropen *po = malloc (sizeof (struct trivfs_peropen));

  if (!po)
    return ENOMEM;

  ports_port_ref (cntl);

  po->refcnt = 1;
  po->cntl = cntl;
  po->openmodes = flags;
  po->hook = 0;

  if (trivfs_peropen_create_hook)
    err = (*trivfs_peropen_create_hook) (po);
  if (!err)
    {
      struct trivfs_protid *new =
	ports_allocate_port (cntl->protid_bucket,
			     sizeof (struct trivfs_protid),
			     cntl->protid_class);

      if (new)
	{
	  int i;

	  new->isroot = 0;
	  for (i = 0; i < num_uids; i++)
	    if (uids[i] == 0)
	      new->isroot = 1;

	  new->uids = malloc (num_uids * sizeof (uid_t));
	  bcopy (uids, new->uids, num_uids * sizeof (uid_t));
	  new->nuids = num_uids;

	  new->gids = malloc (num_gids * sizeof (uid_t));
	  bcopy (gids, new->gids, num_gids * sizeof (uid_t));
	  new->ngids = num_gids;

	  new->po = po;
	  new->hook = 0;
	  new->realnode = realnode;

	  if (!err && trivfs_protid_create_hook)
	    err = (*trivfs_protid_create_hook) (new);

	  if (err)
	    {
	      /* Setting REALNODE to null signals the clean routine not to
		 call the destroy hook, which we deallocate below anyway.  */
	      new->realnode = MACH_PORT_NULL;
	      ports_port_deref (new);
	    }
	  else
	    *cred = new;
	}
      else
	err = ENOMEM;
    }

  if (err)
    {
      ports_port_deref (cntl);
      free (po);
    }

  return err;
}
