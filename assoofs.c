#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO  */
#include <linux/init.h>         /* Needed for the macros */
#include <linux/fs.h>           /* libfs stuff           */
#include <asm/uaccess.h>        /* copy_to_user          */
#include <linux/buffer_head.h>  /* buffer_head           */
#include <linux/slab.h>         /* kmem_cache            */
#include "assoofs.h"

static struct kmem_cache *assoofs_inode_cache;

static struct dentry *assoofs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);
extern struct dentry * d_make_root(struct inode *);
static inline void d_add(struct dentry *entry, struct inode *inode);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);

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

/* Operaciones del superbloque */
static const struct super_operations assoofs_sops = {
	.drop_inode = generic_delete_inode,
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
	sb->s_maxbytes = ASSOOFS_DEFAULT_BLOCK_SIZE;
	sb->s_op = &assoofs_sops;
	// guardar info bloque 0 en campo s_fs_info de sb??? donde está ese campo
	sb->s_fs_info = assoofs_sb;	// el campo ese aunque no lo veamos está en el padding


	/* Creamos el inodo raíz */
	root_inode = new_inode(sb);
	// asignamos propietario y permisos
	inode_init_owner(root_inode, NULL, S_IFDIR);	//S_IFDIR para directorios, S_IFREG para ficheros
	root_inode->i_ino = ASSOOFS_ROOTDIR_INODE_NUMBER;
	root_inode->i_sb = sb;	// puntero al superbloque
	root_inode->i_op = &assoofs_inode_ops;
	root_inode->i_fop = &assoofs_file_operations;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = current_time(root_inode);	// o CURRENT_TIME en duda
	root_inode->i_private = assoofs_get_inode_info(sb, ASSOOFS_ROOTDIR_INODE_NUMBER);	// Información persistente del inodo

	// ToDO Marcar el nuevo inodo como raiz y lo guardaremos en superbloque
	sb->s_root = d_make_root(root_inode);

	if(!sb->s_root){
		d_add(dentry, inode);
	}

}

struct assoofs_inode_info *assoofs_get_inode_info(struct super_block *sb, uint64_t inode_no){

	// Acceder al disco para leer el bloque que contiene el almacen de inodos
	struct assoofs_inode_info *inode_info = NULL;
	struct buffer_head *bh;

	bh = sb_bread(sb, ASSOOFS_INODESTORE_BLOCK_NUMBER);
	inode_info = (struct assoofs_inode_info *)bh->b_data;

	// recorrer el almacen de inodos en busca del inode_no
	struct assoofs_super_block_info *afs_sb = sb->s_fs_info;
	struct assoofs_inode:info *buffer = NULL;

	int i;
	for(i = 0; i < afs_sb->inodes_count; i++){
		if(inode_info->inode_no == inode_no){
			buffer = kmalloc(sizeof(struct assoofs_inode_info), GFP_KERNEL);
			memcpy(buffer, inode_info, sizeof(*buffer));
			break;
		}
		inode_info++;
	}

	// liberar recursos y devolver la info del inodo inode_no si estaba en el almacen
	brelse(bh);
	return buffer;
}

// 2.3.3.4 declarar structs de 2.3.4. y 2.3.5.
// Declarar structs para manejar operaciones de inodos
static struct inode_operations assoofs_inode_ops = {
	.lookup = assoofs_lookup,
	.create = assoofs_create,
	.mkdir = assoofs_mkdir,
};

// Para manejar directorios
const struct dir_operations assoofs_dir_operations = {
	.owner = THIS_MODULE,
	.iterate = assoofs_iterate,
};

// Para manejar ficheros
const struct file_operations assoofs_file_operations = {
	.read = assoofs_read;
	.write = assoofs_write;
};

// 2.3.4
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags){

	struct assoofs_inode_info *parent_info = parent_inode->i_private;
	struct super_block *sb = parent_inode->i_sb;
	struct buffer_head *bh;

	bh = sb_bread(sb, parent_info->data_block_number);

	struct assoofs_inode_info *parent_info = parent_inode->i_private;
	int i;
	for (i = 0; i < parent_info->dir_children_count; i++) {

		if (!strcmp(record->filename, child_dentry->d_name.name)) {

			struct inode *inode = assoofs_get_inode(sb, record->inode_no);
			inode_init_owner(inode, parent_inode, ((struct assoofs_inode_info *)inode->i_private)->mode);
			d_add(child_dentry, inode);
			return NULL;
		}
		record++;
	}

}

static struct inode *assoofs_get_inode(struct super_block *sb, int ino){
	// implementar
	struct inode *inode;

	inode_info = assoofs_get_inode_info(sb, ino);

	

	if (S_ISDIR(inode_info->mode))
		inode->i_fop = &assoofs_dir_operations;
	else if (S_ISREG(inode_info->mode))
		inode->i_fop = &assoofs_file_operations;
	else
		printk(KERN_ERR
					 "Unknown inode type. Neither a directory nor a file");

	/* FIXME: We should store these times to disk and retrieve them */
	inode->i_atime = inode->i_mtime = inode->i_ctime =
			current_time(inode);

	inode->i_private = inode_info;

return inode;
}

// assoofs_create funcion



module_init(assoofs_init);
module_exit(assoofs_exit);