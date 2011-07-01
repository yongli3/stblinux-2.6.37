/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

extern const field_t	attr_flds[];
extern const field_t	attr_hfld[];
extern const field_t	attr_blkinfo_flds[];
extern const field_t	attr_leaf_entry_flds[];
extern const field_t	attr_leaf_hdr_flds[];
extern const field_t	attr_leaf_map_flds[];
extern const field_t	attr_leaf_name_flds[];
extern const field_t	attr_node_entry_flds[];
extern const field_t	attr_node_hdr_flds[];

extern int	attr_leaf_name_size(void *obj, int startoff, int idx);
extern int	attr_size(void *obj, int startoff, int idx);