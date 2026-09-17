/*
 * fat.c
 *
 * R/O (V)FAT 12/16/32 filesystem implementation by Marcus Sundberg
 *
 * 2002-07-28 - rjones@nexus-tech.net - ported to ppcboot v1.1.6
 * 2003-03-10 - kharris@nexus-tech.net - ported to uboot
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <config.h>
#include <fat.h>
#include <asm/byteorder.h>
#include <part.h>

/*
 * Convert a string to lowercase.
 */
static void
downcase(char *str)
{
	while (*str != '\0') {
		TOLOWER(*str);
		str++;
	}
}

static  block_dev_desc_t *cur_dev = NULL;
static unsigned long part_offset = 0;
static int cur_part = 1;

#define DOS_PART_TBL_OFFSET	0x1be
#define DOS_PART_MAGIC_OFFSET	0x1fe
#define DOS_FS_TYPE_OFFSET	0x36

int disk_read (__u32 startblock, __u32 getsize, __u8 * bufptr)
{
	startblock += part_offset;
	if (cur_dev == NULL)
		return -1;
	if (cur_dev->block_read) {
		return cur_dev->block_read (cur_dev->dev
			, startblock, getsize, (unsigned long *)bufptr);
	}
	return -1;
}


int
fat_register_device(block_dev_desc_t *dev_desc, int part_no)
{
	unsigned char buffer[4096];
	disk_partition_t info;

	if (!dev_desc->block_read)
		return -1;
	cur_dev = dev_desc;
	/* check if we have a MBR (on floppies we have only a PBR) */
	if (dev_desc->block_read (dev_desc->dev, 0, 1, (ulong *) buffer) != 1) {
		printf ("** Can't read from device %d **\n", dev_desc->dev);
		return -1;
	}
	if (buffer[DOS_PART_MAGIC_OFFSET] != 0x55 ||
		buffer[DOS_PART_MAGIC_OFFSET + 1] != 0xaa) {
		/* no signature found */
		return -1;
	}
#if (defined(CONFIG_CMD_IDE) || \
     defined(CONFIG_CMD_MG_DISK) || \
     defined(CONFIG_CMD_SATA) || \
     defined(CONFIG_CMD_SCSI) || \
     defined(CONFIG_CMD_USB) || \
     defined(CONFIG_MMC) || \
     defined(CONFIG_SYSTEMACE) )
	/* First we assume, there is a MBR */
	if (!get_partition_info (dev_desc, part_no, &info)) {
		part_offset = info.start;
		cur_part = part_no;
	} else if (!strncmp((char *)&buffer[DOS_FS_TYPE_OFFSET], "FAT", 3)) {
		/* ok, we assume we are on a PBR only */
		cur_part = 1;
		part_offset = 0;
	} else {
        //  MTK Notice: Max Liao, 2007/08/31.
        //  usb disk may not have MBR in disk.
        //  MTK Notice: Jethro Chang, 2010/06/02.
        //  memory card may not have MBR in disk.        
        part_offset = 0;
        cur_part = 1;
        if (dev_desc->if_type == IF_TYPE_USB || dev_desc->if_type == IF_TYPE_MMC)
        {
            return 0;
        }
		printf ("** Partition %d not valid on device %d **\n",
				part_no, dev_desc->dev);
		return -1;
	}

#else
	if (!strncmp((char *)&buffer[DOS_FS_TYPE_OFFSET],"FAT",3)) {
		/* ok, we assume we are on a PBR only */
		cur_part = 1;
		part_offset = 0;
		info.start = part_offset;
	} else {
		/* FIXME we need to determine the start block of the
		 * partition where the DOS FS resides. This can be done
		 * by using the get_partition_info routine. For this
		 * purpose the libpart must be included.
		 */
		part_offset = 32;
		cur_part = 1;
	}
#endif
	return 0;
}


/*
 * Get the first occurence of a directory delimiter ('/' or '\') in a string.
 * Return index into string if found, -1 otherwise.
 */
static int
dirdelim(char *str)
{
	char *start = str;

	while (*str != '\0') {
		if (ISDIRDELIM(*str)) return str - start;
		str++;
	}
	return -1;
}

/*
 * Extract zero terminated short name from a directory entry.
 */
static void get_name (dir_entry *dirent, char *s_name)
{
	char *ptr;

	memcpy (s_name, dirent->name, 8);
	s_name[8] = '\0';
	ptr = s_name;
	while (*ptr && *ptr != ' ')
		ptr++;
	if (dirent->ext[0] && dirent->ext[0] != ' ') {
		*ptr = '.';
		ptr++;
		memcpy (ptr, dirent->ext, 3);
		ptr[3] = '\0';
		while (*ptr && *ptr != ' ')
			ptr++;
	}
	*ptr = '\0';
	if (*s_name == DELETED_FLAG)
		*s_name = '\0';
	else if (*s_name == aRING)
		*s_name = '?';
	downcase (s_name);
}

/*
 * Get the entry at index 'entry' in a FAT (12/16/32) table.
 * On failure 0x00 is returned.
 */
static __u32
get_fatent(fsdata *mydata, __u32 entry)
{
	__u32 bufnum;
	__u32 offset;
	__u32 ret = 0x00;

	switch (mydata->fatsize) {
	case 32:
		bufnum = entry / FAT32BUFSIZE;
		offset = entry - bufnum * FAT32BUFSIZE;
		break;
	case 16:
		bufnum = entry / FAT16BUFSIZE;
		offset = entry - bufnum * FAT16BUFSIZE;
		break;
	case 12:
		bufnum = entry / FAT12BUFSIZE;
		offset = entry - bufnum * FAT12BUFSIZE;
		break;

	default:
		/* Unsupported FAT size */
		return ret;
	}

	/* Read a new block of FAT entries into the cache. */
	if (bufnum != mydata->fatbufnum) {
		int getsize = FATBUFBLOCKS;
		__u8 *bufptr = mydata->fatbuf;
		__u32 fatlength = mydata->fatlength;
		__u32 startblock = bufnum * FATBUFBLOCKS;

		fatlength *= mydata->sector_size;	/* We want it in bytes now */
		startblock += mydata->fat_sect;	/* Offset from start of disk */

		if (getsize > fatlength) getsize = fatlength;
		if (disk_read(startblock, getsize, bufptr) < 0) {
			FAT_DPRINT("Error reading FAT blocks\n");
			return ret;
		}
		mydata->fatbufnum = bufnum;
	}

	/* Get the actual entry from the table */
	switch (mydata->fatsize) {
	case 32:
		ret = FAT2CPU32(((__u32*)mydata->fatbuf)[offset]);
		break;
	case 16:
		ret = FAT2CPU16(((__u16*)mydata->fatbuf)[offset]);
		break;
	case 12: {
		__u32 off16 = (offset*3)/4;
		__u16 val1, val2;

		switch (offset & 0x3) {
		case 0:
			ret = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);
			ret &= 0xfff;
			break;
		case 1:
			val1 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);
			val1 &= 0xf000;
			val2 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16+1]);
			val2 &= 0x00ff;
			ret = (val2 << 4) | (val1 >> 12);
			break;
		case 2:
			val1 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);
			val1 &= 0xff00;
			val2 = FAT2CPU16(((__u16*)mydata->fatbuf)[off16+1]);
			val2 &= 0x000f;
			ret = (val2 << 8) | (val1 >> 8);
			break;
		case 3:
			ret = FAT2CPU16(((__u16*)mydata->fatbuf)[off16]);;
			ret = (ret & 0xfff0) >> 4;
			break;
		default:
			break;
		}
	}
	break;
	}
	//FAT_DPRINT("ret: %d, offset: %d\n", ret, offset);

	return ret;
}


/*
 * Read at most 'size' bytes from the specified cluster into 'buffer'.
 * Return 0 on success, -1 otherwise.
 */
static int
get_cluster(fsdata *mydata, __u32 clustnum, __u8 *buffer, unsigned long size)
{
	int idx = 0;
	__u32 startsect;

	if (clustnum > 0) {
		startsect = mydata->data_begin + clustnum*mydata->clust_size;
	} else {
		startsect = mydata->rootdir_sect;
	}

	FAT_DPRINT("gc - clustnum: %d, startsect: %d\n", clustnum, startsect);
	if (disk_read(startsect, size/mydata->sector_size , buffer) < 0) {
		FAT_DPRINT("Error reading data\n");
		return -1;
	}
	if(size % mydata->sector_size) {
		__u8 tmpbuf[8192];
		idx= size/mydata->sector_size;
		if (disk_read(startsect + idx, 1, tmpbuf) < 0) {
			FAT_DPRINT("Error reading data\n");
			return -1;
		}
		buffer += idx*mydata->sector_size;

		memcpy(buffer, tmpbuf, size % mydata->sector_size);
		return 0;
	}

	return 0;
}


/*
 * Read at most 'maxsize' bytes from the file associated with 'dentptr'
 * into 'buffer'.
 * Return the number of bytes read or -1 on fatal errors.
 */
static long
get_contents(fsdata *mydata, dir_entry *dentptr, __u8 *buffer,
	     unsigned long maxsize)
{
	unsigned long filesize = FAT2CPU32(dentptr->size), gotsize = 0;
	unsigned int bytesperclust = mydata->clust_size * mydata->sector_size;
	__u32 curclust = START(dentptr);
	__u32 endclust, newclust;
	unsigned long actsize;

	FAT_DPRINT("Filesize: %ld bytes\n", filesize);

	if (maxsize > 0 && filesize > maxsize) filesize = maxsize;

	FAT_DPRINT("Reading: %ld bytes\n", filesize);

	actsize=bytesperclust;
	endclust=curclust;
	do {
		/* search for consecutive clusters */
		while(actsize < filesize) {
			newclust = get_fatent(mydata, endclust);
			if((newclust -1)!=endclust)
				goto getit;
#if 0
			if (CHECK_CLUST(newclust, mydata->fatsize)) {
				FAT_DPRINT("curclust: 0x%x\n", newclust);
				FAT_DPRINT("Invalid FAT entry\n");
				return gotsize;
			}
#endif
			endclust=newclust;
			actsize+= bytesperclust;
		}
		/* actsize >= file size */
		actsize -= bytesperclust;
		/* get remaining clusters */
		if (get_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
			FAT_ERROR("Error reading cluster\n");
			return -1;
		}
		/* get remaining bytes */
		gotsize += (int)actsize;
		filesize -= actsize;
		buffer += actsize;
		actsize= filesize;
		if (get_cluster(mydata, endclust, buffer, (int)actsize) != 0) {
			FAT_ERROR("Error reading cluster\n");
			return -1;
		}
		gotsize+=actsize;
		return gotsize;
getit:
		if (get_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
			FAT_ERROR("Error reading cluster\n");
			return -1;
		}
		gotsize += (int)actsize;
		filesize -= actsize;
		buffer += actsize;
		curclust = get_fatent(mydata, endclust);
		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			FAT_DPRINT("curclust: 0x%x\n", curclust);
			FAT_ERROR("Invalid FAT entry\n");
			return gotsize;
		}
		actsize=bytesperclust;
		endclust=curclust;
	} while (1);
}

static unsigned long
get_contents_ex(fsdata *mydata, dir_entry *dentptr, __u8 *buffer,
	     unsigned long maxsize, unsigned long pos)
{
	unsigned long filesize = FAT2CPU32(dentptr->size), gotsize = 0;
	unsigned int bytesperclust = mydata->clust_size * mydata->sector_size;
	__u32 curclust = START(dentptr);
	__u32 endclust, newclust;
	unsigned long actsize;

	FAT_DPRINT("Filesize: %ld bytes\n", filesize);

	if (maxsize > 0 && filesize > maxsize) filesize = maxsize;

	FAT_DPRINT("Reading: %ld bytes\n", filesize);
   
	actsize=bytesperclust;
	endclust=curclust;
	do {
		/* search for consecutive clusters */
		while(actsize < (filesize + pos)) {
			newclust = get_fatent(mydata, endclust);
			if((newclust -1)!=endclust)
			{
				FAT_DPRINT("[YL line=415]newclust= %lu != endclust = %lu\n",newclust, endclust);
				goto getit;
			}
#if 0
			if (CHECK_CLUST(newclust, mydata->fatsize)) {
				FAT_DPRINT("curclust: 0x%x\n", newclust);
				FAT_DPRINT("Invalid FAT entry\n");
				return gotsize;
			}
#endif
			endclust=newclust;
			actsize+= bytesperclust;
		}

		FAT_DPRINT("[YL line =428]actsize = %lu, pos = %lu, newclust = %lu, endclust = %lu\n",actsize,pos,newclust,endclust);
		if (pos != 0)     /* read first part which isn't aligned with start of sector */
		{
		    __u8 tmpbuf[65536];
			curclust += (pos / bytesperclust);
			pos &= bytesperclust - 1;
			FAT_DPRINT("[YL line =434]bytesperclust = %lu, pos = %lu, filesize = %lu, gotsize = %lu, mydata->sector_size = %lu\n",bytesperclust,pos,filesize,gotsize,mydata->sector_size);
			if (get_cluster(mydata, curclust, tmpbuf, bytesperclust) != 0)
			{
				FAT_ERROR("Error reading cluster\n");
				return -1;
			}
			memcpy(buffer, tmpbuf + pos, min(bytesperclust - pos, filesize));
			buffer += min(bytesperclust - pos, filesize);
			gotsize += min(bytesperclust - pos, filesize);
			filesize -= min(bytesperclust - pos, filesize);
			curclust++;
			pos = 0;
		}
		FAT_DPRINT("[YL line = 447] filesize = %lu, gotsize = %lu\n",filesize,gotsize);
		if (filesize == 0)
		{
			return gotsize;
		}
		
		/* get remaining clusters */
		if (get_cluster(mydata, curclust, buffer, filesize) != 0) {
			FAT_ERROR("Error reading cluster\n");
			return -1;
		}
		FAT_DPRINT("[YL line = 458] filesize = %lu, gotsize = %lu\n",filesize,gotsize);
		/* get remaining bytes */
		gotsize += filesize;
		return gotsize;
getit:
	    if(pos != 0)
	    {
	        if(actsize > (filesize + pos))
	        {
				__u8 tmpbuf[65536];
				FAT_DPRINT("[YL line = 468] actsize = %lu, pos = %lu\n",actsize,pos);
				curclust += (pos / bytesperclust);
				pos &= bytesperclust - 1;

				if (get_cluster(mydata, curclust, tmpbuf, bytesperclust) != 0)
				{
					FAT_ERROR("Error reading cluster\n");
					return -1;
				}
				memcpy(buffer, tmpbuf + pos, min(bytesperclust - pos, filesize));
				buffer += min(bytesperclust - pos, filesize);
				gotsize += min(bytesperclust - pos, filesize);
				filesize -= min(bytesperclust - pos, filesize);
				curclust++;
				pos = 0;

				if (filesize == 0)
				{
					return gotsize;
				}
				
				/* get remaining clusters */
				if (get_cluster(mydata, curclust, buffer, filesize) != 0) {
					FAT_ERROR("Error reading cluster\n");
					return -1;
				}
				/* get remaining bytes */
				gotsize += filesize;
				return gotsize;
			}
			if(( pos < actsize ) && (actsize <= (filesize + pos)))
			{
				__u8 tmpbuf[65536];				
				FAT_DPRINT("[YL line = 501] actsize = %lu, pos = %lu\n",actsize,pos);
				actsize -= pos;
				curclust += (pos / bytesperclust);
				pos &= bytesperclust - 1;

				if (get_cluster(mydata, curclust, tmpbuf, bytesperclust) != 0)
				{
					FAT_ERROR("Error reading cluster\n");
					return -1;
				}
				memcpy(buffer, tmpbuf + pos, min(bytesperclust - pos, actsize));
				buffer += min(bytesperclust - pos, actsize);
				actsize -= min(bytesperclust - pos, actsize);
				gotsize += min(bytesperclust - pos, actsize);
				filesize -= min(bytesperclust - pos, actsize);
				curclust++;
				pos = 0;
							
				/* get remaining clusters */
				if (get_cluster(mydata, curclust, buffer, actsize) != 0) {
					FAT_ERROR("Error reading cluster\n");
					return -1;
				}
				/* get remaining bytes */
				gotsize += actsize;
				filesize -= actsize;
				buffer += actsize;
				
			}
			else
			{
				FAT_DPRINT("[YL line = 531] actsize = %lu, pos = %lu\n",actsize,pos);
				pos -= actsize;
			}
		}
		else
		{	
			FAT_DPRINT("[YL line = 537] actsize = %lu, pos = %lu, curclust = %lu\n",actsize,pos,curclust);
			if (get_cluster(mydata, curclust, buffer, (int)actsize) != 0) {
				FAT_ERROR("Error reading cluster\n");
				return -1;
			}
			gotsize += (int)actsize;
			filesize -= actsize;
			buffer += actsize;
		}
		FAT_DPRINT("[YL line = 546] actsize = %lu, pos = %lu, curclust = %lu\n",actsize,pos,curclust);
		curclust = get_fatent(mydata, endclust);
		if (CHECK_CLUST(curclust, mydata->fatsize)) {
			FAT_DPRINT("curclust: 0x%x\n", curclust);
			FAT_ERROR("Invalid FAT entry\n");
			return gotsize;
		}
		actsize=bytesperclust;
		endclust=curclust;
		
	} while (1);
}


#ifdef CONFIG_SUPPORT_VFAT
/*
 * Extract the file name information from 'slotptr' into 'l_name',
 * starting at l_name[*idx].
 * Return 1 if terminator (zero byte) is found, 0 otherwise.
 */
static int
slot2str(dir_slot *slotptr, char *l_name, int *idx)
{
	int j;

	for (j = 0; j <= 8; j += 2) {
		l_name[*idx] = slotptr->name0_4[j];
		if (l_name[*idx] == 0x00) return 1;
		(*idx)++;
	}
	for (j = 0; j <= 10; j += 2) {
		l_name[*idx] = slotptr->name5_10[j];
		if (l_name[*idx] == 0x00) return 1;
		(*idx)++;
	}
	for (j = 0; j <= 2; j += 2) {
		l_name[*idx] = slotptr->name11_12[j];
		if (l_name[*idx] == 0x00) return 1;
		(*idx)++;
	}

	return 0;
}


/*
 * Extract the full long filename starting at 'retdent' (which is really
 * a slot) into 'l_name'. If successful also copy the real directory entry
 * into 'retdent'
 * Return 0 on success, -1 otherwise.
 */
__attribute__ ((__aligned__(__alignof__(dir_entry))))
__u8 get_vfatname_block[MAX_CLUSTSIZE];
static int
get_vfatname(fsdata *mydata, int curclust, __u8 *cluster,
	     dir_entry *retdent, char *l_name)
{
	dir_entry *realdent;
	dir_slot  *slotptr = (dir_slot*) retdent;
        dir_slot *slotptr2 = NULL;	
	__u8	  *nextclust = cluster + mydata->clust_size * mydata->sector_size;
	__u8	   counter = (slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff;
	int idx = 0;
	int cursect = 0;

	while ((__u8*)slotptr < nextclust) {
		if (counter == 0) break;
		if (((slotptr->id & ~LAST_LONG_ENTRY_MASK) & 0xff) != counter)
			return -1;
		slotptr++;
		counter--;
	}

	if (((__u8*)slotptr >= nextclust) && 
	    ((slotptr->id > 0x01) && (slotptr->attr == ATTR_VFAT))) 
	    {
        	switch (mydata->fatsize) 
        	{
                    case 32:                         
		curclust = get_fatent(mydata, curclust);
                         if ((curclust & 0x0FFFFFFF) == 0x0FFFFFFF)
                         {
			return -1;
		}
		if (get_cluster(mydata, curclust, get_vfatname_block,
        				mydata->clust_size * mydata->sector_size) != 0) {
			FAT_DPRINT("Error: reading directory block\n");
			return -1;
		}             	                   	          	
                	
                break;
                case 16:
                case 12:
                     cursect = mydata->clust_size*(curclust-2+1) + mydata->rootdir_sect;
                     if (disk_read (cursect, mydata->clust_size, get_vfatname_block) < 0) 
                     {
                         FAT_DPRINT ("Error: reading rootdir block\n");
                	 return -1;
                     }

                     break;
                default:
                     return -1;
                }
		
		slotptr2 = (dir_slot*) get_vfatname_block;
		while (slotptr2->id > 0x01) {
			slotptr2++;
		}
		/* Save the real directory entry */
		realdent = (dir_entry*)slotptr2 + 1;
		while ((__u8*)slotptr2 >= get_vfatname_block) {
			slot2str(slotptr2, l_name, &idx);
			slotptr2--;
		}
	} else {
		/* Save the real directory entry */
		realdent = (dir_entry*)slotptr;
	}

	do {
		slotptr--;
		if (slot2str(slotptr, l_name, &idx)) break;
	} while (!(slotptr->id & LAST_LONG_ENTRY_MASK));

	l_name[idx] = '\0';
	if (*l_name == DELETED_FLAG) *l_name = '\0';
	else if (*l_name == aRING) *l_name = '?';
	downcase(l_name);

	/* Return the real directory entry */
	memcpy(retdent, realdent, sizeof(dir_entry));

	return 0;
}


/* Calculate short name checksum */
static __u8
mkcksum(const char *str)
{
	int i;
	__u8 ret = 0;

	for (i = 0; i < 11; i++) {
		ret = (((ret&1)<<7)|((ret&0xfe)>>1)) + str[i];
	}

	return ret;
}
#endif


/*
 * Get the directory entry associated with 'filename' from the directory
 * starting at 'startsect'
 */
__attribute__ ((__aligned__(__alignof__(dir_entry))))
__u8 get_dentfromdir_block[MAX_CLUSTSIZE];
static dir_entry *get_dentfromdir (fsdata * mydata, int startsect,
				   char *filename, dir_entry * retdent,
				   int dols)
{
    __u16 prevcksum = 0xffff;
    __u32 curclust = START (retdent);
    int files = 0, dirs = 0;

    FAT_DPRINT ("get_dentfromdir: %s\n", filename);
    while (1) {
	dir_entry *dentptr;
	int i;

	if (get_cluster (mydata, curclust, get_dentfromdir_block,
		 mydata->clust_size * mydata->sector_size) != 0) {
	    FAT_DPRINT ("Error: reading directory block\n");
	    return NULL;
	}
	dentptr = (dir_entry *) get_dentfromdir_block;
	for (i = 0; i < DIRENTSPERCLUST; i++) {
	    char s_name[14], l_name[256];

	    l_name[0] = '\0';
	    if (dentptr->name[0] == DELETED_FLAG) {
		    dentptr++;
		    continue;
	    }
	    if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef CONFIG_SUPPORT_VFAT
		if ((dentptr->attr & ATTR_VFAT) &&
		    (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
		    prevcksum = ((dir_slot *) dentptr)
			    ->alias_checksum;
		    get_vfatname (mydata, curclust, get_dentfromdir_block,
				  dentptr, l_name);
		    if (dols) {
			int isdir = (dentptr->attr & ATTR_DIR);
			char dirc;
			int doit = 0;

			if (isdir) {
			    dirs++;
			    dirc = '/';
			    doit = 1;
			} else {
			    dirc = ' ';
			    if (l_name[0] != 0) {
				files++;
				doit = 1;
			    }
			}
			if (doit) {
			    if (dirc == ' ') {
				printf (" %8ld   %s%c\n",
					(long) FAT2CPU32 (dentptr->size),
					l_name, dirc);
			    } else {
				printf ("            %s%c\n", l_name, dirc);
			    }
			}
			dentptr++;
			continue;
		    }
		    FAT_DPRINT ("vfatname: |%s|\n", l_name);
		} else
#endif
		{
		    /* Volume label or VFAT entry */
		    dentptr++;
		    continue;
		}
	    }
	    if (dentptr->name[0] == 0) {
		if (dols) {
		    printf ("\n%d file(s), %d dir(s)\n\n", files, dirs);
		}
		FAT_DPRINT ("Dentname == NULL - %d\n", i);
		return NULL;
	    }
#ifdef CONFIG_SUPPORT_VFAT
	    if (dols && mkcksum (dentptr->name) == prevcksum) {
		dentptr++;
		continue;
	    }
#endif
	    get_name (dentptr, s_name);
	    if (dols) {
		int isdir = (dentptr->attr & ATTR_DIR);
		char dirc;
		int doit = 0;

		if (isdir) {
		    dirs++;
		    dirc = '/';
		    doit = 1;
		} else {
		    dirc = ' ';
		    if (s_name[0] != 0) {
			files++;
			doit = 1;
		    }
		}
		if (doit) {
		    if (dirc == ' ') {
			printf (" %8ld   %s%c\n",
				(long) FAT2CPU32 (dentptr->size), s_name,
				dirc);
		    } else {
			printf ("            %s%c\n", s_name, dirc);
		    }
		}
		dentptr++;
		continue;
	    }
	    if (strcmp (filename, s_name) && strcmp (filename, l_name)) {
		FAT_DPRINT ("Mismatch: |%s|%s|\n", s_name, l_name);
		dentptr++;
		continue;
	    }
	    memcpy (retdent, dentptr, sizeof (dir_entry));

	    FAT_DPRINT ("DentName: %s", s_name);
	    FAT_DPRINT (", start: 0x%x", START (dentptr));
	    FAT_DPRINT (", size:  0x%x %s\n",
			FAT2CPU32 (dentptr->size),
			(dentptr->attr & ATTR_DIR) ? "(DIR)" : "");

	    return retdent;
	}
	curclust = get_fatent (mydata, curclust);
	if (CHECK_CLUST(curclust, mydata->fatsize)) {
	    FAT_DPRINT ("curclust: 0x%x\n", curclust);
	    FAT_ERROR ("Invalid FAT entry\n");
	    return NULL;
	}
    }

    return NULL;
}


/*
 * Read boot sector and volume info from a FAT filesystem
 */
static int
read_bootsectandvi(boot_sector *bs, volume_info *volinfo, int *fatsize)
{
	__u8 block[4096];
	volume_info *vistart;

	if (disk_read(0, 1, block) < 0) {
		FAT_DPRINT("Error: reading block\n");
		return -1;
	}

	memcpy(bs, block, sizeof(boot_sector));
	bs->reserved	= FAT2CPU16(bs->reserved);
	bs->fat_length	= FAT2CPU16(bs->fat_length);
	bs->secs_track	= FAT2CPU16(bs->secs_track);
	bs->heads	= FAT2CPU16(bs->heads);
#if 0 /* UNUSED */
	bs->hidden	= FAT2CPU32(bs->hidden);
#endif
	bs->total_sect	= FAT2CPU32(bs->total_sect);

	/* FAT32 entries */
	if (bs->fat_length == 0) {
		/* Assume FAT32 */
		bs->fat32_length = FAT2CPU32(bs->fat32_length);
		bs->flags	 = FAT2CPU16(bs->flags);
		bs->root_cluster = FAT2CPU32(bs->root_cluster);
		bs->info_sector  = FAT2CPU16(bs->info_sector);
		bs->backup_boot  = FAT2CPU16(bs->backup_boot);
		vistart = (volume_info*) (block + sizeof(boot_sector));
		*fatsize = 32;
	} else {
		vistart = (volume_info*) &(bs->fat32_length);
		*fatsize = 0;
	}
	memcpy(volinfo, vistart, sizeof(volume_info));

	if (*fatsize == 32) {
		if (strncmp(FAT32_SIGN, vistart->fs_type, SIGNLEN) == 0) {
			return 0;
		}
	} else {
		if (strncmp(FAT12_SIGN, vistart->fs_type, SIGNLEN) == 0) {
			*fatsize = 12;
			return 0;
		}
		if (strncmp(FAT16_SIGN, vistart->fs_type, SIGNLEN) == 0) {
			*fatsize = 16;
			return 0;
		}
	}

	FAT_DPRINT("Error: broken fs_type sign\n");
	return -1;
}

__attribute__ ((__aligned__(__alignof__(dir_entry))))
__u8 do_fat_read_block[MAX_CLUSTSIZE];
long
do_fat_read (const char *filename, void *buffer, unsigned long maxsize,
	     int dols)
{
#if CONFIG_NIOS /* NIOS CPU cannot access big automatic arrays */
    static
#endif
    char fnamecopy[4096];
    boot_sector bs;
    volume_info volinfo;
    fsdata datablock;
    fsdata *mydata = &datablock;
    dir_entry *dentptr;
    __u16 prevcksum = 0xffff;
    char *subname = "";
    int rootdir_size, cursect, first_datasect;
    int idx, isdir = 0;
    int files = 0, dirs = 0;
    long ret = 0;
    int firsttime;
    __u32 curclust = 2;

    if (read_bootsectandvi (&bs, &volinfo, &mydata->fatsize)) {
	FAT_DPRINT ("Error: reading boot sector\n");
	return -1;
    }
    if (mydata->fatsize == 32) {
	mydata->fatlength = bs.fat32_length;
    } else {
	mydata->fatlength = bs.fat_length;
    }
    mydata->fat_sect = bs.reserved;
    cursect = mydata->rootdir_sect
	    = mydata->fat_sect + mydata->fatlength * bs.fats;
    first_datasect = cursect;
    mydata->sector_size = (bs.sector_size[1] << 8) |(bs.sector_size[0]);
    mydata->clust_size = bs.cluster_size;
    if (mydata->fatsize == 32) {
	rootdir_size = mydata->clust_size;
	mydata->data_begin = mydata->rootdir_sect   /* + rootdir_size */
		- (mydata->clust_size * 2);
    } else {
	rootdir_size = ((bs.dir_entries[1] * (int) 256 + bs.dir_entries[0])
			* sizeof (dir_entry)) / mydata->sector_size;
	mydata->data_begin = mydata->rootdir_sect + rootdir_size
		- (mydata->clust_size * 2);
    }
    mydata->fatbufnum = -1;

    FAT_DPRINT ("FAT%d, fatlength: %d\n", mydata->fatsize,
		mydata->fatlength);
    FAT_DPRINT ("Rootdir begins at sector: %d, offset: %x, size: %d\n"
		"Data begins at: %d\n",
		mydata->rootdir_sect, mydata->rootdir_sect * mydata->sector_size,
		rootdir_size, mydata->data_begin);
    FAT_DPRINT ("Cluster size: %d, Sector size: %d.\n", 
             mydata->clust_size, mydata->sector_size);

    /* "cwd" is always the root... */
    while (ISDIRDELIM (*filename))
	filename++;
    /* Make a copy of the filename and convert it to lowercase */
    strcpy (fnamecopy, filename);
    downcase (fnamecopy);
    if (*fnamecopy == '\0') {
	if (!dols)
	    return -1;
	dols = LS_ROOT;
    } else if ((idx = dirdelim (fnamecopy)) >= 0) {
	isdir = 1;
	fnamecopy[idx] = '\0';
	subname = fnamecopy + idx + 1;
	/* Handle multiple delimiters */
	while (ISDIRDELIM (*subname))
	    subname++;
    } else if (dols) {
	isdir = 1;
    }

    while (1) {
	int i;

	if (disk_read (cursect, mydata->clust_size, do_fat_read_block) < 0) {
	    FAT_DPRINT ("Error: reading rootdir block\n");
	    return -1;
	}
	dentptr = (dir_entry *) do_fat_read_block;
	for (i = 0; i < (DIRENTSPERBLOCK) * mydata->clust_size; i++) {
	    char s_name[14], l_name[256];

	    l_name[0] = '\0';
	    if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef CONFIG_SUPPORT_VFAT
		if (((dentptr->attr & ATTR_VFAT) == ATTR_VFAT) &&
		    (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
		    prevcksum = ((dir_slot *) dentptr)->alias_checksum;
		    get_vfatname (mydata, curclust, do_fat_read_block, dentptr, l_name);
		    if (dols == LS_ROOT) {
			int isdir = (dentptr->attr & ATTR_DIR);
			char dirc;
			int doit = 0;

			if (isdir) {
			    dirs++;
			    dirc = '/';
			    doit = 1;
			} else {
			    dirc = ' ';
			    if (l_name[0] != 0) {
				files++;
				doit = 1;
			    }
			}
			if (doit) {
			    if (dirc == ' ') {
				printf (" %8ld   %s%c\n",
					(long) FAT2CPU32 (dentptr->size),
					l_name, dirc);
			    } else {
				printf ("            %s%c\n", l_name, dirc);
			    }
			}
			dentptr++;
			continue;
		    }
		    FAT_DPRINT ("Rootvfatname: |%s|\n", l_name);
		} else
#endif
		{
		    /* Volume label or VFAT entry */
		    dentptr++;
		    continue;
		}
	    } else if (dentptr->name[0] == 0) {
		FAT_DPRINT ("RootDentname == NULL - %d\n", i);
		if (dols == LS_ROOT) {
		    printf ("\n%d file(s), %d dir(s)\n\n", files, dirs);
		    return 0;
		}
		return -1;
	    }
#ifdef CONFIG_SUPPORT_VFAT
	    else if (dols == LS_ROOT
		     && mkcksum (dentptr->name) == prevcksum) {
		dentptr++;
		continue;
	    }
#endif
	    get_name (dentptr, s_name);
	    if (dols == LS_ROOT) {
		int isdir = (dentptr->attr & ATTR_DIR);
		char dirc;
		int doit = 0;

		if (isdir) {
		    dirc = '/';
		    if (s_name[0] != 0) {
			dirs++;
			doit = 1;
		    }
		} else {
		    dirc = ' ';
		    if (s_name[0] != 0) {
			files++;
			doit = 1;
		    }
		}
		if (doit) {
		    if (dirc == ' ') {
			printf (" %8ld   %s%c\n",
				(long) FAT2CPU32 (dentptr->size), s_name,
				dirc);
		    } else {
			printf ("            %s%c\n", s_name, dirc);
		    }
		}
		dentptr++;
		continue;
	    }
	    if (strcmp (fnamecopy, s_name) && strcmp (fnamecopy, l_name)) {
		//FAT_DPRINT ("RootMismatch: |%s|%s|\n", s_name, l_name);
		dentptr++;
		continue;
	    }
	    if (isdir && !(dentptr->attr & ATTR_DIR))
		return -1;

	    FAT_DPRINT ("RootName: %s", s_name);
	    FAT_DPRINT (", start: 0x%x", START (dentptr));
	    FAT_DPRINT (", size:  0x%x %s\n",
			FAT2CPU32 (dentptr->size), isdir ? "(DIR)" : "");

	    goto rootdir_done;  /* We got a match */
	}
	switch (mydata->fatsize) 
	{
            case 32:                         
            	curclust = get_fatent(mydata, curclust);
        	if ((curclust & 0x0FFFFFFF) == 0x0FFFFFFF)
        	{
        	    FAT_DPRINT ("RootDentname == NULL - %d\n", i);
                    return -1;
        	}                	
                cursect = (curclust - 2) *mydata->clust_size + first_datasect;
            	break;
            case 16:
            case 12: 
                curclust ++;
                cursect += mydata->clust_size;
                rootdir_size -= mydata->clust_size;
                if (rootdir_size < 0)
                {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }
  rootdir_done:

    firsttime = 1;
    while (isdir) {
	int startsect = mydata->data_begin
		+ START (dentptr) * mydata->clust_size;
	dir_entry dent;
	char *nextname = NULL;

	dent = *dentptr;
	dentptr = &dent;

	idx = dirdelim (subname);
	if (idx >= 0) {
	    subname[idx] = '\0';
	    nextname = subname + idx + 1;
	    /* Handle multiple delimiters */
	    while (ISDIRDELIM (*nextname))
		nextname++;
	    if (dols && *nextname == '\0')
		firsttime = 0;
	} else {
	    if (dols && firsttime) {
		firsttime = 0;
	    } else {
		isdir = 0;
	    }
	}

	if (get_dentfromdir (mydata, startsect, subname, dentptr,
			     isdir ? 0 : dols) == NULL) {
	    if (dols && !isdir)
		return 0;
	    return -1;
	}

	if (idx >= 0) {
	    if (!(dentptr->attr & ATTR_DIR))
		return -1;
	    subname = nextname;
	}
    }
    ret = get_contents (mydata, dentptr, buffer, maxsize);
    FAT_DPRINT ("Size: %d, got: %ld\n", FAT2CPU32 (dentptr->size), ret);

    return ret;
}

long
do_fat_read_length (const char *filename, int dols)
{
#if CONFIG_NIOS /* NIOS CPU cannot access big automatic arrays */
    static
#endif
    unsigned long u4_file_length = 0;
    char fnamecopy[4096];
    boot_sector bs;
    volume_info volinfo;
    fsdata datablock;
    fsdata *mydata = &datablock;
    dir_entry *dentptr;
    __u16 prevcksum = 0xffff;
    char *subname = "";
    int rootdir_size, cursect, first_datasect;
    int idx, isdir = 0;
    int files = 0, dirs = 0;
    long ret = 0;
    int firsttime;
    __u32 curclust = 2;

    if (read_bootsectandvi (&bs, &volinfo, &mydata->fatsize)) {
	FAT_DPRINT ("Error: reading boot sector\n");
	return -1;
    }
    if (mydata->fatsize == 32) {
	mydata->fatlength = bs.fat32_length;
    } else {
	mydata->fatlength = bs.fat_length;
    }
    mydata->fat_sect = bs.reserved;
    cursect = mydata->rootdir_sect
	    = mydata->fat_sect + mydata->fatlength * bs.fats;
    first_datasect = cursect;
    mydata->sector_size = (bs.sector_size[1] << 8) |(bs.sector_size[0]);
    mydata->clust_size = bs.cluster_size;
    if (mydata->fatsize == 32) {
	rootdir_size = mydata->clust_size;
	mydata->data_begin = mydata->rootdir_sect   /* + rootdir_size */
		- (mydata->clust_size * 2);
    } else {
	rootdir_size = ((bs.dir_entries[1] * (int) 256 + bs.dir_entries[0])
			* sizeof (dir_entry)) / mydata->sector_size;
	mydata->data_begin = mydata->rootdir_sect + rootdir_size
		- (mydata->clust_size * 2);
    }
    mydata->fatbufnum = -1;

    FAT_DPRINT ("FAT%d, fatlength: %d\n", mydata->fatsize,
		mydata->fatlength);
    FAT_DPRINT ("Rootdir begins at sector: %d, offset: %x, size: %d\n"
		"Data begins at: %d\n",
		mydata->rootdir_sect, mydata->rootdir_sect * mydata->sector_size,
		rootdir_size, mydata->data_begin);
    FAT_DPRINT ("Cluster size: %d, Sector size: %d.\n", 
             mydata->clust_size, mydata->sector_size);

    /* "cwd" is always the root... */
    while (ISDIRDELIM (*filename))
	filename++;
    /* Make a copy of the filename and convert it to lowercase */
    strcpy (fnamecopy, filename);
    downcase (fnamecopy);
    if (*fnamecopy == '\0') {
	if (!dols)
	    return -1;
	dols = LS_ROOT;
    } else if ((idx = dirdelim (fnamecopy)) >= 0) {
	isdir = 1;
	fnamecopy[idx] = '\0';
	subname = fnamecopy + idx + 1;
	/* Handle multiple delimiters */
	while (ISDIRDELIM (*subname))
	    subname++;
    } else if (dols) {
	isdir = 1;
    }

    while (1) {
	int i;

	if (disk_read (cursect, mydata->clust_size, do_fat_read_block) < 0) {
	    FAT_DPRINT ("Error: reading rootdir block\n");
	    return -1;
	}
	dentptr = (dir_entry *) do_fat_read_block;
	for (i = 0; i < (DIRENTSPERBLOCK) * mydata->clust_size; i++) {
	    char s_name[14], l_name[256];

	    l_name[0] = '\0';
	    if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef CONFIG_SUPPORT_VFAT
		if (((dentptr->attr & ATTR_VFAT) == ATTR_VFAT) &&
		    (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
		    prevcksum = ((dir_slot *) dentptr)->alias_checksum;
		    get_vfatname (mydata, curclust, do_fat_read_block, dentptr, l_name);
		    if (dols == LS_ROOT) {
			int isdir = (dentptr->attr & ATTR_DIR);
			char dirc;
			int doit = 0;

			if (isdir) {
			    dirs++;
			    dirc = '/';
			    doit = 1;
			} else {
			    dirc = ' ';
			    if (l_name[0] != 0) {
				files++;
				doit = 1;
			    }
			}
			if (doit) {
			    if (dirc == ' ') {
				printf (" %8ld   %s%c\n",
					(long) FAT2CPU32 (dentptr->size),
					l_name, dirc);
			    } else {
				printf ("            %s%c\n", l_name, dirc);
			    }
			}
			dentptr++;
			continue;
		    }
		    FAT_DPRINT ("Rootvfatname: |%s|\n", l_name);
		} else
#endif
		{
		    /* Volume label or VFAT entry */
		    dentptr++;
		    continue;
		}
	    } else if (dentptr->name[0] == 0) {
		FAT_DPRINT ("RootDentname == NULL - %d\n", i);
		if (dols == LS_ROOT) {
		    printf ("\n%d file(s), %d dir(s)\n\n", files, dirs);
		    return 0;
		}
		return -1;
	    }
#ifdef CONFIG_SUPPORT_VFAT
	    else if (dols == LS_ROOT
		     && mkcksum (dentptr->name) == prevcksum) {
		dentptr++;
		continue;
	    }
#endif
	    get_name (dentptr, s_name);
	    if (dols == LS_ROOT) {
		int isdir = (dentptr->attr & ATTR_DIR);
		char dirc;
		int doit = 0;

		if (isdir) {
		    dirc = '/';
		    if (s_name[0] != 0) {
			dirs++;
			doit = 1;
		    }
		} else {
		    dirc = ' ';
		    if (s_name[0] != 0) {
			files++;
			doit = 1;
		    }
		}
		if (doit) {
		    if (dirc == ' ') {
			printf (" %8ld   %s%c\n",
				(long) FAT2CPU32 (dentptr->size), s_name,
				dirc);
		    } else {
			printf ("            %s%c\n", s_name, dirc);
		    }
		}
		dentptr++;
		continue;
	    }
	    if (strcmp (fnamecopy, s_name) && strcmp (fnamecopy, l_name)) {
		//FAT_DPRINT ("RootMismatch: |%s|%s|\n", s_name, l_name);
		dentptr++;
		continue;
	    }
	    if (isdir && !(dentptr->attr & ATTR_DIR))
		return -1;

	    FAT_DPRINT ("RootName: %s", s_name);
	    FAT_DPRINT (", start: 0x%x", START (dentptr));
	    FAT_DPRINT (", size:  0x%x %s\n",
			FAT2CPU32 (dentptr->size), isdir ? "(DIR)" : "");

	    goto rootdir_done;  /* We got a match */
	}
	switch (mydata->fatsize) 
	{
            case 32:                         
            	curclust = get_fatent(mydata, curclust);
        	if ((curclust & 0x0FFFFFFF) == 0x0FFFFFFF)
        	{
        	    FAT_DPRINT ("RootDentname == NULL - %d\n", i);
                    return -1;
        	}                	
                cursect = (curclust - 2) *mydata->clust_size + first_datasect;
            	break;
            case 16:
            case 12: 
                curclust ++;
                cursect += mydata->clust_size;
                rootdir_size -= mydata->clust_size;
                if (rootdir_size < 0)
                {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }
  rootdir_done:

    firsttime = 1;
    while (isdir) {
	int startsect = mydata->data_begin
		+ START (dentptr) * mydata->clust_size;
	dir_entry dent;
	char *nextname = NULL;

	dent = *dentptr;
	dentptr = &dent;

	idx = dirdelim (subname);
	if (idx >= 0) {
	    subname[idx] = '\0';
	    nextname = subname + idx + 1;
	    /* Handle multiple delimiters */
	    while (ISDIRDELIM (*nextname))
		nextname++;
	    if (dols && *nextname == '\0')
		firsttime = 0;
	} else {
	    if (dols && firsttime) {
		firsttime = 0;
	    } else {
		isdir = 0;
	    }
	}

	if (get_dentfromdir (mydata, startsect, subname, dentptr,
			     isdir ? 0 : dols) == NULL) {
	    if (dols && !isdir)
		return 0;
	    return -1;
	}

	if (idx >= 0) {
	    if (!(dentptr->attr & ATTR_DIR))
		return -1;
	    subname = nextname;
	}
    }
    u4_file_length = FAT2CPU32 (dentptr->size);
	printf("u4_file_length = %lu\n", u4_file_length);
    FAT_DPRINT ("Size: %lu, got: %ld\n", u4_file_length, ret);

    return u4_file_length;
}

long
do_fat_read_ex (const char *filename, void *buffer, unsigned long maxsize,
	     int dols, unsigned long pos)
{
#if CONFIG_NIOS /* NIOS CPU cannot access big automatic arrays */
    static
#endif
    char fnamecopy[4096];
    boot_sector bs;
    volume_info volinfo;
    fsdata datablock;
    fsdata *mydata = &datablock;
    dir_entry *dentptr;
    __u16 prevcksum = 0xffff;
    char *subname = "";
    int rootdir_size, cursect, first_datasect;
    int idx, isdir = 0;
    int files = 0, dirs = 0;
    unsigned long ret = 0;
    int firsttime;
    __u32 curclust = 2;

    if (read_bootsectandvi (&bs, &volinfo, &mydata->fatsize)) {
	FAT_DPRINT ("Error: reading boot sector\n");
	return -1;
    }
    if (mydata->fatsize == 32) {
	mydata->fatlength = bs.fat32_length;
    } else {
	mydata->fatlength = bs.fat_length;
    }
    mydata->fat_sect = bs.reserved;
    cursect = mydata->rootdir_sect
	    = mydata->fat_sect + mydata->fatlength * bs.fats;
    first_datasect = cursect;
    mydata->sector_size = (bs.sector_size[1] << 8) |(bs.sector_size[0]);
    mydata->clust_size = bs.cluster_size;
    if (mydata->fatsize == 32) {
	rootdir_size = mydata->clust_size;
	mydata->data_begin = mydata->rootdir_sect   /* + rootdir_size */
		- (mydata->clust_size * 2);
    } else {
	rootdir_size = ((bs.dir_entries[1] * (int) 256 + bs.dir_entries[0])
			* sizeof (dir_entry)) / mydata->sector_size;
	mydata->data_begin = mydata->rootdir_sect + rootdir_size
		- (mydata->clust_size * 2);
    }
    mydata->fatbufnum = -1;
    FAT_DPRINT ("FAT%d, fatlength: %d\n", mydata->fatsize,
		mydata->fatlength);
    FAT_DPRINT ("Rootdir begins at sector: %d, offset: %x, size: %d\n"
		"Data begins at: %d\n",
		mydata->rootdir_sect, mydata->rootdir_sect * mydata->sector_size,
		rootdir_size, mydata->data_begin);
    FAT_DPRINT ("Cluster size: %d, Sector size: %d.\n", 
             mydata->clust_size, mydata->sector_size);

    /* "cwd" is always the root... */
    while (ISDIRDELIM (*filename))
	filename++;
    /* Make a copy of the filename and convert it to lowercase */
    strcpy (fnamecopy, filename);
    downcase (fnamecopy);
    if (*fnamecopy == '\0') {
	if (!dols)
	    return -1;
	dols = LS_ROOT;
    } else if ((idx = dirdelim (fnamecopy)) >= 0) {
	isdir = 1;
	fnamecopy[idx] = '\0';
	subname = fnamecopy + idx + 1;
	/* Handle multiple delimiters */
	while (ISDIRDELIM (*subname))
	    subname++;
    } else if (dols) {
	isdir = 1;
    }

    while (1) {
	int i;

	if (disk_read (cursect, mydata->clust_size, do_fat_read_block) < 0) {
	    FAT_DPRINT ("Error: reading rootdir block\n");
	    return -1;
	}
	dentptr = (dir_entry *) do_fat_read_block;
	for (i = 0; i < (DIRENTSPERBLOCK) * mydata->clust_size; i++) {
	    char s_name[14], l_name[256];

	    l_name[0] = '\0';
	    if ((dentptr->attr & ATTR_VOLUME)) {
#ifdef CONFIG_SUPPORT_VFAT
		if (((dentptr->attr & ATTR_VFAT) == ATTR_VFAT) &&
		    (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) {
		    prevcksum = ((dir_slot *) dentptr)->alias_checksum;
		    get_vfatname (mydata, curclust, do_fat_read_block, dentptr, l_name);
		    if (dols == LS_ROOT) {
			int isdir = (dentptr->attr & ATTR_DIR);
			char dirc;
			int doit = 0;

			if (isdir) {
			    dirs++;
			    dirc = '/';
			    doit = 1;
			} else {
			    dirc = ' ';
			    if (l_name[0] != 0) {
				files++;
				doit = 1;
			    }
			}
			if (doit) {
			    if (dirc == ' ') {
				printf (" %8ld   %s%c\n",
					(long) FAT2CPU32 (dentptr->size),
					l_name, dirc);
			    } else {
				printf ("            %s%c\n", l_name, dirc);
			    }
			}
			dentptr++;
			continue;
		    }
		    FAT_DPRINT ("Rootvfatname: |%s|\n", l_name);
		} else
#endif
		{
		    /* Volume label or VFAT entry */
		    dentptr++;
		    continue;
		}
	    } else if (dentptr->name[0] == 0) {
		FAT_DPRINT ("RootDentname == NULL - %d\n", i);
		if (dols == LS_ROOT) {
		    printf ("\n%d file(s), %d dir(s)\n\n", files, dirs);
		    return 0;
		}
		return -1;
	    }
#ifdef CONFIG_SUPPORT_VFAT
	    else if (dols == LS_ROOT
		     && mkcksum (dentptr->name) == prevcksum) {
		dentptr++;
		continue;
	    }
#endif
	    get_name (dentptr, s_name);
	    if (dols == LS_ROOT) {
		int isdir = (dentptr->attr & ATTR_DIR);
		char dirc;
		int doit = 0;

		if (isdir) {
		    dirc = '/';
		    if (s_name[0] != 0) {
			dirs++;
			doit = 1;
		    }
		} else {
		    dirc = ' ';
		    if (s_name[0] != 0) {
			files++;
			doit = 1;
		    }
		}
		if (doit) {
		    if (dirc == ' ') {
			printf (" %8ld   %s%c\n",
				(long) FAT2CPU32 (dentptr->size), s_name,
				dirc);
		    } else {
			printf ("            %s%c\n", s_name, dirc);
		    }
		}
		dentptr++;
		continue;
	    }
	    if (strcmp (fnamecopy, s_name) && strcmp (fnamecopy, l_name)) {
		//FAT_DPRINT ("RootMismatch: |%s|%s|\n", s_name, l_name);
		dentptr++;
		continue;
	    }
	    if (isdir && !(dentptr->attr & ATTR_DIR))
		return -1;

	    FAT_DPRINT ("RootName: %s", s_name);
	    FAT_DPRINT (", start: 0x%x", START (dentptr));
	    FAT_DPRINT (", size:  0x%x %s\n",
			FAT2CPU32 (dentptr->size), isdir ? "(DIR)" : "");

	    goto rootdir_done;  /* We got a match */
	}
	switch (mydata->fatsize) 
	{
            case 32:                         
            	curclust = get_fatent(mydata, curclust);
        	if ((curclust & 0x0FFFFFFF) == 0x0FFFFFFF)
        	{
        	    FAT_DPRINT ("RootDentname == NULL - %d\n", i);
                    return -1;
        	}                	
                cursect = (curclust - 2) *mydata->clust_size + first_datasect;
            	break;
            case 16:
            case 12: 
                curclust ++;
                cursect += mydata->clust_size;
                rootdir_size -= mydata->clust_size;
                if (rootdir_size < 0)
                {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }
  rootdir_done:

    firsttime = 1;
    while (isdir) {
	int startsect = mydata->data_begin
		+ START (dentptr) * mydata->clust_size;
	dir_entry dent;
	char *nextname = NULL;

	dent = *dentptr;
	dentptr = &dent;

	idx = dirdelim (subname);
	if (idx >= 0) {
	    subname[idx] = '\0';
	    nextname = subname + idx + 1;
	    /* Handle multiple delimiters */
	    while (ISDIRDELIM (*nextname))
		nextname++;
	    if (dols && *nextname == '\0')
		firsttime = 0;
	} else {
	    if (dols && firsttime) {
		firsttime = 0;
	    } else {
		isdir = 0;
	    }
	}

	if (get_dentfromdir (mydata, startsect, subname, dentptr,
			     isdir ? 0 : dols) == NULL) {
	    if (dols && !isdir)
		return 0;
	    return -1;
	}

	if (idx >= 0) {
	    if (!(dentptr->attr & ATTR_DIR))
		return -1;
	    subname = nextname;
	}
    }
    ret = get_contents_ex (mydata, dentptr, buffer, maxsize, pos);
    FAT_DPRINT ("Size: %lu, got: %lu\n", FAT2CPU32 (dentptr->size), ret);

    return ret;
}


int
file_fat_detectfs(void)
{
	boot_sector	bs;
	volume_info	volinfo;
	int		fatsize;
	char	vol_label[12];

	if(cur_dev==NULL) {
		printf("No current device\n");
		return 1;
	}
#if defined(CONFIG_CMD_IDE) || \
    defined(CONFIG_CMD_MG_DISK) || \
    defined(CONFIG_CMD_SATA) || \
    defined(CONFIG_CMD_SCSI) || \
    defined(CONFIG_CMD_USB) || \
    defined(CONFIG_MMC)
	printf("Interface:  ");
	switch(cur_dev->if_type) {
		case IF_TYPE_IDE :	printf("IDE"); break;
		case IF_TYPE_SATA :	printf("SATA"); break;
		case IF_TYPE_SCSI :	printf("SCSI"); break;
		case IF_TYPE_ATAPI :	printf("ATAPI"); break;
		case IF_TYPE_USB :	printf("USB"); break;
		case IF_TYPE_DOC :	printf("DOC"); break;
		case IF_TYPE_MMC :	printf("MMC"); break;
		default :		printf("Unknown");
	}
	printf("\n  Device %d: ",cur_dev->dev);
	dev_print(cur_dev);
#endif
	if(read_bootsectandvi(&bs, &volinfo, &fatsize)) {
		printf("\nNo valid FAT fs found\n");
		return 1;
	}
	memcpy (vol_label, volinfo.volume_label, 11);
	vol_label[11] = '\0';
	volinfo.fs_type[5]='\0';
	printf("Partition %d: Filesystem: %s \"%s\"\n"
			,cur_part,volinfo.fs_type,vol_label);
	return 0;
}


int
file_fat_ls(const char *dir)
{
	return do_fat_read(dir, NULL, 0, LS_YES);
}


long
file_fat_read(const char *filename, void *buffer, unsigned long maxsize)
{
	printf("reading %s\n",filename);
	return do_fat_read(filename, buffer, maxsize, LS_NO);
}

long
file_fat_read_ex(const char *filename, void *buffer, unsigned long maxsize, unsigned long pos)
{
	printf("reading %s\n",filename);
	return do_fat_read_ex(filename, buffer, maxsize, LS_NO, pos);
}


//mtk40224 follow code for match a file with '*" ----------
int fat_namestrcmp(const char *str1, const char *str2)
{
	const unsigned char *s1 = (const unsigned char *)str1;
	const unsigned char *s2 = (const unsigned char *)str2;
	int delta = 0;

	while (*s1 || *s2) {
		delta = *s2 - *s1;
        if (*s2 == '*')  //match '*' character
        {
            delta = 0;
        }
		if (delta)
        {
			return delta;
        }
		s1++;
		s2++;
	}
	return 0;
}

static dir_entry *get_dentfromdir_formatchfile (fsdata * mydata, int startsect,
				   char *dirname,char *matchfile, char *filename, dir_entry * retdent,
				   int dols)
{
    __u16 prevcksum = 0xffff;
    __u32 curclust = START (retdent);
    int files = 0, dirs = 0;

    FAT_DPRINT ("get_dentfromdir: %s\n", dirname);
    while (1) 
    {
	    dir_entry *dentptr;
   	    int i;

	    if (get_cluster (mydata, curclust, get_dentfromdir_block,
		    mydata->clust_size * mydata->sector_size) != 0) 
        {
	        FAT_DPRINT ("Error: reading directory block\n");
	        return NULL;
	    }
	    dentptr = (dir_entry *) get_dentfromdir_block;
	    for (i = 0; i < DIRENTSPERCLUST; i++)
	    {
	        char s_name[14], l_name[256];

	        l_name[0] = '\0';
	        if (dentptr->name[0] == DELETED_FLAG)
	        {
		        dentptr++;
		        continue;
	        }
	        if ((dentptr->attr & ATTR_VOLUME))
	        {
#ifdef CONFIG_SUPPORT_VFAT
		        if ((dentptr->attr & ATTR_VFAT) &&
		            (dentptr->name[0] & LAST_LONG_ENTRY_MASK))
		        {
		            prevcksum = ((dir_slot *) dentptr)->alias_checksum;
		            get_vfatname (mydata, curclust, get_dentfromdir_block,
				                  dentptr, l_name);
		            if (dols)
		            {
			            int isdir = (dentptr->attr & ATTR_DIR);
			            char dirc;
			            int doit = 0;

			            if (isdir) 
			            {
			                dirs++;
			                dirc = '/';
			                doit = 1;
			            }
			            else
			            {
			                dirc = ' ';
			                if (l_name[0] != 0)
			                {
				                files++;
				                doit = 1;
			                }
			            }
		  	            if (doit)
		  	            {
			                if (dirc == ' ')
			                {				                
                                if (!fat_namestrcmp(l_name, matchfile))
					            {
                                    strcpy (filename, l_name);
                                    return retdent;
                                }
			                }			                
			            }
			            dentptr++;
			            continue;
		            }
		            FAT_DPRINT ("vfatname: |%s|\n", l_name);
		        }
		        else
#endif
		        {
		            /* Volume label or VFAT entry */
		            dentptr++;
		            continue;
		        }
	        }
	        if (dentptr->name[0] == 0)
	        {
		        if (dols)
		        {
		            FAT_DPRINT ("\n%d file(s), %d dir(s)\n\n", files, dirs);
		        }
		        FAT_DPRINT ("Dentname == NULL - %d\n", i);
		        return NULL;
	        }
#ifdef CONFIG_SUPPORT_VFAT
	        if (dols && mkcksum (dentptr->name) == prevcksum) 
	        {
		        dentptr++;
		        continue;
	        }
#endif
	        get_name (dentptr, s_name);
	        if (dols)
	        {
		        int isdir = (dentptr->attr & ATTR_DIR);
		        char dirc;
		        int doit = 0;

		        if (isdir)
		        {
		            dirs++;
		            dirc = '/';
		            doit = 1;
		        }
		        else
		        {
		            dirc = ' ';
		            if (s_name[0] != 0)
		            {
			            files++;
			            doit = 1;
		            }
		        }
		        if (doit)
		        {
		            if (dirc == ' ')
		            {			            
                        if (!fat_namestrcmp(s_name, matchfile))
					    {
                            strcpy (filename, s_name);
                            return retdent;
                        }
		            }		            
		        }
		        dentptr++;
		        continue;
	        }
	        if (strcmp (dirname, s_name) && strcmp (dirname, l_name))
	        {
		        FAT_DPRINT ("Mismatch: |%s|%s|\n", s_name, l_name);
		        dentptr++;
		        continue;
	        }
	        memcpy (retdent, dentptr, sizeof (dir_entry));

	        FAT_DPRINT ("DentName: %s", s_name);
	        FAT_DPRINT (", start: 0x%x", START (dentptr));
	        FAT_DPRINT (", size:  0x%x %s\n", FAT2CPU32 (dentptr->size),(dentptr->attr & ATTR_DIR) ? "(DIR)" : "");

	        return retdent;
	    }
	    curclust = get_fatent (mydata, curclust);
	    if (CHECK_CLUST(curclust, mydata->fatsize)) 
	    {
	        FAT_DPRINT ("curclust: 0x%x\n", curclust);
	        FAT_ERROR ("Invalid FAT entry\n");
	        return NULL;
     	}
    }

    return NULL;
}

long do_fat_read_from_dir (const char *dirname, const char *matchfile, char *outfilename, unsigned long maxsize, int dols)
{    
#if CONFIG_NIOS /* NIOS CPU cannot access big automatic arrays */
    static
#endif
    char fnamecopy[4096];
    char fmatchfilecopy[256];
    boot_sector bs;
    volume_info volinfo;
    fsdata datablock;
    fsdata *mydata = &datablock;
    dir_entry *dentptr;
    __u16 prevcksum = 0xffff;
    char *subname = "";
    int rootdir_size, cursect, first_datasect;
    int idx, isdir = 0;
    int files = 0, dirs = 0;
    long ret = 0;
    int firsttime;
    __u32 curclust = 2;

    if (read_bootsectandvi (&bs, &volinfo, &mydata->fatsize))
    {
	   FAT_DPRINT ("Error: reading boot sector\n");
 	   return -1;
    }
    if (mydata->fatsize == 32)
    {
	    mydata->fatlength = bs.fat32_length;
    }
    else 
    {
	    mydata->fatlength = bs.fat_length;
    }    
    mydata->fat_sect = bs.reserved;
    cursect = mydata->rootdir_sect
	    = mydata->fat_sect + mydata->fatlength * bs.fats;
    first_datasect = cursect;
    mydata->sector_size = (bs.sector_size[1] << 8) |(bs.sector_size[0]);
    mydata->clust_size = bs.cluster_size;
    if (mydata->fatsize == 32)
    {
	    rootdir_size = mydata->clust_size;
   	    mydata->data_begin = mydata->rootdir_sect   /* + rootdir_size */
		       - (mydata->clust_size * 2);
    } 
    else 
    {
	    rootdir_size = ((bs.dir_entries[1] * (int) 256 + bs.dir_entries[0])
			   * sizeof (dir_entry)) / mydata->sector_size;
	    mydata->data_begin = mydata->rootdir_sect + rootdir_size
		       - (mydata->clust_size * 2);
    }
    
    mydata->fatbufnum = -1;

    
    FAT_DPRINT ("FAT%d, fatlength: %d\n", mydata->fatsize,
		mydata->fatlength);
    FAT_DPRINT ("Rootdir begins at sector: %d, offset: %x, size: %d\n"
		"Data begins at: %d\n",
		mydata->rootdir_sect, mydata->rootdir_sect * mydata->sector_size,
		rootdir_size, mydata->data_begin);
    FAT_DPRINT ("Cluster size: %d, Sector size: %d.\n", 
             mydata->clust_size, mydata->sector_size);

    //printf("HM_TEST_01 .\n");
    /* "cwd" is always the root... */
    while (ISDIRDELIM (*dirname))
	    dirname++;
    /* Make a copy of the dirname and convert it to lowercase */
    strcpy (fnamecopy, dirname);
    downcase (fnamecopy);

    strncpy(fmatchfilecopy, matchfile, 256);
    downcase(fmatchfilecopy);
    
    if (*fnamecopy == '\0') 
    {
	    if (!dols)
	        return -1;
	    dols = LS_ROOT;
    }
    else if ((idx = dirdelim (fnamecopy)) >= 0) 
    {
	    isdir = 1;
	    fnamecopy[idx] = '\0';
	    subname = fnamecopy + idx + 1;
	    /* Handle multiple delimiters */
    	while (ISDIRDELIM (*subname))
	        subname++;
    } 
    else if (dols)
    {
	    isdir = 1;
    }
    
    while (1)
    {
	    int i;
        
	    if (disk_read (cursect, mydata->clust_size, do_fat_read_block) < 0)
	    {
	        FAT_DPRINT ("Error: reading rootdir block\n");
	        return -1;
	    }
	    dentptr = (dir_entry *) do_fat_read_block;
	    for (i = 0; i < (DIRENTSPERBLOCK) * mydata->clust_size; i++)
	    {
	        char s_name[14], l_name[256];

	        l_name[0] = '\0';
	        if ((dentptr->attr & ATTR_VOLUME))
	        {
#ifdef CONFIG_SUPPORT_VFAT
		        if (((dentptr->attr & ATTR_VFAT) == ATTR_VFAT) &&
		           (dentptr->name[0] & LAST_LONG_ENTRY_MASK)) 
		        {
		            prevcksum = ((dir_slot *) dentptr)->alias_checksum;
		            get_vfatname (mydata, curclust, do_fat_read_block, dentptr, l_name);
		            if (dols == LS_ROOT)
		            {
			            int isdir = (dentptr->attr & ATTR_DIR);
			            char dirc;
			            int doit = 0;

			            if (isdir) 
			            {
			                dirs++;
			                dirc = '/';
			                doit = 1;
			            } 
			            else 
			            {
			                dirc = ' ';
			                if (l_name[0] != 0)
			                {
				                files++;
				                doit = 1;
			                }
			            }
			            if (doit)
			            {
			                if (dirc == ' ')
			                {			         	        
					            if (!fat_namestrcmp(l_name, fmatchfilecopy))
					            {
					                //printf("HM_TEST_02 .\n");
					                strcpy (outfilename, l_name);
					                return 0;
					            }
			                }			                
			            }
			            dentptr++;
			            continue;
		            }
		            FAT_DPRINT ("Rootvfatname: |%s|\n", l_name);
		        } 
		        else
#endif
		        {
		            /* Volume label or VFAT entry */
		            dentptr++;
		            continue;
		        }
	        }
	        else if (dentptr->name[0] == 0) 
	        {
		        FAT_DPRINT ("RootDentname == NULL - %d\n", i);
		        if (dols == LS_ROOT)
		        {
		            FAT_DPRINT ("\n%d file(s), %d dir(s)\n\n", files, dirs);
		            return 0;
		        }
		        return -1;
	        }
#ifdef CONFIG_SUPPORT_VFAT
	        else if (dols == LS_ROOT
		        && mkcksum (dentptr->name) == prevcksum)
		    {
		        dentptr++;
		        continue;
	        }
#endif
	        get_name (dentptr, s_name);
	        if (dols == LS_ROOT)
	        {
		        int isdir = (dentptr->attr & ATTR_DIR);
		        char dirc;
	       	    int doit = 0;

		        if (isdir) 
		        {
		            dirc = '/';
		            if (s_name[0] != 0)
		            {
		          	   dirs++;
			           doit = 1;
		            }
		        } 
		        else
		        {
		            dirc = ' ';
		            if (s_name[0] != 0)
		            {
			            files++;
			            doit = 1;
		            }
		        }
		        if (doit)
		        {
		            if (dirc == ' ') 
		            {			            
                        if (!fat_namestrcmp(l_name, fmatchfilecopy))
					    {
					        //printf("HM_TEST_03 .\n");
                            strcpy (outfilename, l_name);
                            return 0;
                        }
		            }		            
		        }
	         	dentptr++;
		        continue;
	        }
	        if (strcmp (fnamecopy, s_name) && strcmp (fnamecopy, l_name)) 
	        {
		        //FAT_DPRINT ("RootMismatch: |%s|%s|\n", s_name, l_name);
		        dentptr++;
		        continue;
	        }
	        if (isdir && !(dentptr->attr & ATTR_DIR))
		        return -1;

	        FAT_DPRINT ("RootName: %s", s_name);
	        FAT_DPRINT (", start: 0x%x", START (dentptr));
	        FAT_DPRINT (", size:  0x%x %s\n",
			FAT2CPU32 (dentptr->size), isdir ? "(DIR)" : "");

	        goto rootdir_done;  /* We got a match */
	    }
	    switch (mydata->fatsize) 
	    {	        
            case 32:                         
            	curclust = get_fatent(mydata, curclust);
        	    if ((curclust & 0x0FFFFFFF) == 0x0FFFFFFF)
        	    {
        	        FAT_DPRINT ("RootDentname == NULL - %d\n", i);
                    return -1;
             	}                	
                cursect = (curclust - 2) *mydata->clust_size + first_datasect;
            	break;
            case 16:
            case 12: 
                curclust ++;
                cursect += mydata->clust_size;
                rootdir_size -= mydata->clust_size;
                if (rootdir_size < 0)
                {
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }
  rootdir_done:    
    firsttime = 1;
    while (isdir)
    {        
	    int startsect = mydata->data_begin
		    + START (dentptr) * mydata->clust_size;
	    dir_entry dent;
	    char *nextname = NULL;

	    dent = *dentptr;
	    dentptr = &dent;

	    idx = dirdelim (subname);
	    if (idx >= 0)
	    {
	        subname[idx] = '\0';
	        nextname = subname + idx + 1;
	        /* Handle multiple delimiters */
	        while (ISDIRDELIM (*nextname))
		        nextname++;
	        if (dols && *nextname == '\0')
		        firsttime = 0;
	    }
	    else
	    {
	        if (dols && firsttime) 
	        {
		        firsttime = 0;
	        } 
	        else
	        {
		        isdir = 0;
	        }
	    }
        //printf("HM_TEST_04 .\n");
	    if (get_dentfromdir_formatchfile (mydata, startsect, subname, fmatchfilecopy, outfilename,dentptr,
			     isdir ? 0 : dols) == NULL)
        {
	        if (dols && !isdir)
	        {	            
		        return 0;
		    }
		    
	        return -1;
	    }

	    if (idx >= 0)
	    {
	        if (!(dentptr->attr & ATTR_DIR))
		    {		        
		        return -1;
		    }
	        subname = nextname;
	    }
    }    
    FAT_DPRINT ("Size: %d, got: %ld\n", FAT2CPU32 (dentptr->size), ret);    
    return ret;
}

int x_match_file_from_dir(const char *device,const char *part, const char *dir, const char *matchfile, char *outfilename)
{
    int ret;  
	int dev=0;
	char *ep;
	block_dev_desc_t *dev_desc=NULL;   
	
	dev = (int)simple_strtoul (part, &ep, 16);
	dev_desc=get_dev(device,dev);
	if (dev_desc==NULL)
	{
		puts ("\n** Invalid boot device **\n");
		return -1;
    }
	if (*ep)
	{
		if (*ep != ':')
		{
			puts ("\n** Invalid boot device, use `dev[:part]' **\n");
			return -1;
		}
		part = (int)simple_strtoul(++ep, NULL, 16);
	}
	if (fat_register_device(dev_desc,part)!=0)
	{
		FAT_DPRINT ("\n** Unable to use %s %d:%d for match a file **\n",device,dev,part);
		return -1;
	}	
	printf("call do_fat_read_from_dir dir=%s, matchfile=%s .\n", dir,matchfile);
	ret= do_fat_read_from_dir(dir,matchfile, outfilename, 0, LS_YES);

    if (ret != 0)
    {
        ret = -1;
    }
    if (!strcmp(outfilename, ""))
    {
        ret = -1;
    }
    printf("find file name is =%s .\n", outfilename);
	return ret;
}