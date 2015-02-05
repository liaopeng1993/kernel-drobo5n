/*
 * Copyright (c) 1998-2014 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2014 Stony Brook University
 * Copyright (c) 2003-2014 The Research Foundation of SUNY
 * Copyright (c) 2014-2015 Ricardo Padilha for Drobo Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "notifyfs.h"

/*
 * The inode cache is used with alloc_inode for both our inode info and the
 * vfs inode.
 */
static struct kmem_cache *notifyfs_inode_cachep;

/* final actions when unmounting a file system */
static void notifyfs_put_super(struct super_block *sb) {
	struct notifyfs_sb_info *spd;
	struct super_block *s;

	spd = NOTIFYFS_SB(sb);
	if (!spd) {
		return;
	}

	UDBG;

	/* decrement lower super references */
	s = notifyfs_lower_super(sb);
	notifyfs_set_lower_super(sb, NULL);
	atomic_dec(&s->s_active);

	/* notifier support */
	remove_proc_entry(PROC_PID_BLACKLIST_FILE, spd->notifier_info.proc_dir);
	remove_proc_entry(PROC_FIFO_SIZE_FILE, spd->notifier_info.proc_dir);
	remove_proc_entry(PROC_FIFO_BLOCK_FILE, spd->notifier_info.proc_dir);
	remove_proc_entry(PROC_LOCK_MASK_FILE, spd->notifier_info.proc_dir);
	remove_proc_entry(PROC_GLOBAL_LOCK_FILE, spd->notifier_info.proc_dir);
	remove_proc_entry(PROC_EVENT_MASK_FILE, spd->notifier_info.proc_dir);
	remove_proc_entry(PROC_EVENTS_FILE, spd->notifier_info.proc_dir);
	remove_proc_entry(PROC_SRC_DIR_FILE, spd->notifier_info.proc_dir);
	destroy_proc_mount_dir(spd->notifier_info.proc_dir);

	kfifo_free(&spd->notifier_info.fifo);
	int_list_free(&spd->notifier_info.pids);
	/* end notifier support */

	kfree(spd);
	sb->s_fs_info = NULL;
}

static int notifyfs_statfs(struct dentry *dentry, struct kstatfs *buf) {
	int err;
	struct path lower_path;

	UDBG;
	notifyfs_get_lower_path(dentry, &lower_path);
	err = vfs_statfs(&lower_path, buf);
	notifyfs_put_lower_path(dentry, &lower_path);

	/* set return buf to our f/s to avoid confusing user-level utils */
	buf->f_type = NOTIFYFS_SUPER_MAGIC;

	return err;
}

/*
 * @flags: numeric mount options
 * @options: mount options string
 */
static int notifyfs_remount_fs(struct super_block *sb, int *flags,
		char *options) {
	int err = 0;

	UDBG;
	/*
	 * The VFS will take care of "ro" and "rw" flags among others.  We
	 * can safely accept a few flags (RDONLY, MANDLOCK), and honor
	 * SILENT, but anything else left over is an error.
	 */
	if ((*flags & ~(MS_RDONLY | MS_MANDLOCK | MS_SILENT)) != 0) {
		pr_err(NOTIFYFS_NAME ": remount flags 0x%x unsupported\n", *flags);
		err = -EINVAL;
	}

	/* notify remount */
	err = send_mnt_event(sb, FS_MNT_REMOUNT);

	return err;
}

/*
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list.
 */
static void notifyfs_evict_inode(struct inode *inode) {
	struct inode *lower_inode;

	truncate_inode_pages(&inode->i_data, 0);
	end_writeback(inode);
	/*
	 * Decrement a reference to a lower_inode, which was incremented
	 * by our read_inode when it was created initially.
	 */
	lower_inode = notifyfs_lower_inode(inode);
	notifyfs_set_lower_inode(inode, NULL);
	iput(lower_inode);
}

static struct inode *notifyfs_alloc_inode(struct super_block *sb) {
	struct notifyfs_inode_info *i;

	i = kmem_cache_alloc(notifyfs_inode_cachep, GFP_KERNEL);
	if (!i) {
		return NULL;
	}

	/* memset everything up to the inode to 0 */
	memset(i, 0, offsetof(struct notifyfs_inode_info, vfs_inode));

	i->vfs_inode.i_version = 1;
	return &i->vfs_inode;
}

static void notifyfs_destroy_inode(struct inode *inode) {
	kmem_cache_free(notifyfs_inode_cachep, NOTIFYFS_I(inode));
}

/* notifyfs inode cache constructor */
static void init_once(void *obj) {
	struct notifyfs_inode_info *i = obj;

	inode_init_once(&i->vfs_inode);
}

int notifyfs_init_inode_cache(void) {
	int err = 0;

	UDBG;
	notifyfs_inode_cachep = kmem_cache_create("notifyfs_inode_cache",
			sizeof(struct notifyfs_inode_info), 0,
			SLAB_RECLAIM_ACCOUNT, init_once);
	if (!notifyfs_inode_cachep) {
		err = -ENOMEM;
	}
	return err;
}

/* notifyfs inode cache destructor */
void notifyfs_destroy_inode_cache(void) {
	if (notifyfs_inode_cachep) {
		UDBG;
		kmem_cache_destroy(notifyfs_inode_cachep);
	}
}

/*
 * Used only in nfs, to kill any pending RPC tasks, so that subsequent
 * code can actually succeed and won't leave tasks that need handling.
 */
static void notifyfs_umount_begin(struct super_block *sb) {
	struct super_block *lower_sb;

	lower_sb = notifyfs_lower_super(sb);
	if (lower_sb && lower_sb->s_op && lower_sb->s_op->umount_begin) {
		UDBG;
		lower_sb->s_op->umount_begin(lower_sb);
	}
}

static int notifyfs_show_options(struct seq_file *seq, struct vfsmount *mnt) {
	int err = 0;
	struct super_block *sb = mnt->mnt_sb;
	struct notifyfs_sb_info *spd = NOTIFYFS_SB(sb);

	err = seq_printf(seq, ",event_mask=%u", atomic_read(&spd->notifier_info.event_mask));
	if (err < 0) {
		goto out;
	}
	err = seq_printf(seq, ",lock_mask=%u", atomic_read(&spd->notifier_info.lock_mask));
	if (err < 0) {
		goto out;
	}
	err = seq_printf(seq, ",fifo_size=%u", kfifo_size(&spd->notifier_info.fifo));
	if (err < 0) {
		goto out;
	}
	if (atomic_read(&spd->notifier_info.fifo_block) == NONBLOCKING_FIFO) {
		err = seq_puts(seq, ",non_blocking_fifo");
	} else {
		err = seq_puts(seq, ",blocking_fifo");
	}

out:
	return err;
}

static int notifyfs_show_devname(struct seq_file *seq, struct vfsmount *mnt) {
	int err = 0;
	struct super_block *sb = mnt->mnt_sb;
	char *buffer;
	char *name;

	buffer = kzalloc(MAX_PATH_LENGTH, GFP_KERNEL);
	CHECK_PTR(buffer, out);

	name = d_path(&NOTIFYFS_D(sb->s_root)->lower_path, buffer, MAX_PATH_LENGTH);
	CHECK_PTR(buffer, out_free);

	err = seq_escape(seq, name, " \t\n\\");

out_free:
	kfree(buffer);
out:
	return err;
}

const struct super_operations notifyfs_sops = {
	.put_super = notifyfs_put_super,
	.statfs = notifyfs_statfs,
	.remount_fs = notifyfs_remount_fs,
	.evict_inode = notifyfs_evict_inode,
	.umount_begin = notifyfs_umount_begin,
	.alloc_inode = notifyfs_alloc_inode,
	.destroy_inode = notifyfs_destroy_inode,
	.drop_inode = generic_delete_inode,
	.show_options = notifyfs_show_options,
	.show_devname = notifyfs_show_devname
};