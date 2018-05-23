#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <asm/uaccess.h>        /* copy_to_user          */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

static struct kmem_cache *assoofs_inode_cache;

ssize_t assoofs_read(struct file * filp, char __user * buf, size_t len, loff_t * ppos);
ssize_t assoofs_write(struct file * filp, const char __user * buf, size_t len, loff_t * ppos);
//static int assoofs_iterate(struct file *filp, struct dir_context, )	// inacabado

// 2.3.1
static struct file_system_type assoofs_type = {
	.owner = THIS_MODULE,
	.name 	= 'assoofs',
	//.mount	= assoofs_mount,
	.kill_sb = kill_litter_super,
};

static int __init assoofs_init(void){
	int ret;
	assoofs_inode_cache = kmem_cache_create("assoofs_inode_cache", sizeof(struct assoofs_inode_info), 0, (SLAB_RECLAIM_ACCOUNT| SLAB_MEM_SPREAD), NULL);
	if(!assoofs_inode_cache) return -ENOMEM;

	ret = register_filesystem(&assoofs_type);
	if(likely(ret == 0))
		printk(KERN_INFO "Succesfully registered assoofs\n");
	else
		printk(KERN_ERR "Failed to register assoofs. Error %d.", ret);
	return ret;
}

static void __exit assoofs_exit(void){
	int ret;
	ret = unregister_filesystem(&assoofs_type);
	kmem_cache_destroy(assoofs_inode_cache);

	if(likely(ret == 0))
		printk(KERN_INFO "Succesfully unregistered assoofs\n");
	else
		printk(KERN_ERR "Failed to unregister assoofs. Error:[%d]", ret);
}

// 2.3.2 función mount
static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data){

	// los data van a ser persistentes, montemos desmontemos, lo que sea

	struct dentry *ret;
	ret = mount_bdev(fs_type, flags, dev_name, data, assoofs_fill_super);

	if(unlikely(IS_ERR(ret)))
		printk(KERN_ERR "Error mounting assoofs.");
	else
		printk(KERN_INFO "assoofs is succesfully mounted on %s\n", dev_name);
	
	return ret;
}

// 2.3.3 assoofs fill_super
int assoofs_fill_super(struct super_block *sb, void *data, int silent){
	struct inode *root_inode;
	struct buffer_head *bh;
	struct assoofs_super_block_info *assoofs_sb;

	bh = sb_bread(sb, ASSOOFS_SUPERBLOCK_NUMBER);
	assoofs_sb = (struct assoofs_super_block_info *)bh->b_data;

	printk(KERN_INFO "The magic number obtained in disk is %llu\n", assoofs_sb->magic);
	if(unlikely(assoofs_sb->magic != ASSOOFS_MAGIC)){
		printk(KERN_ERR "The filesystem that you try to mount is not of the type assoofs. MAgicnumber mismatch.");
		brelse(bh);
		return -EPERM;
	}
	if(unlikely(assoofs_sb->block_size != ASSOOFS_DEFAULT_BLOCK_SIZE)){
		printk(KERN_ERR "assoofs seem to be formatted using a wrong block size.");
		brelse(bh);
		return -EPERM;
	}
	printk(KERN_INFO "assoofs filesystem of version %llu formatted with a block size of %llu detected in the device.\n", assoofs_sb->version, assoofs_sb->block_size);

	sb->s_magic = ASSOOFS_MAGIC;

	// seguir en casa mirando en el github de las referencias
}

module_init(assoofs_init);
module_exit(assoofs_exit);