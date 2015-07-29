////////////////////////////////////////////////////////////////////////////
//
// linux_drvrint.c                 I    OOO                           h
//                                 I   O   O     t      eee    cccc   h
//                                 I  O     O  ttttt   e   e  c       h hhh
// -----------------------------   I  O     O    t     eeeee  c       hh   h
// copyright: (C) 2002 by IOtech   I   O   O     t     e      c       h    h
//    email: linux@iotech.com      I    OOO       tt    eee    cccc   h    h
//
////////////////////////////////////////////////////////////////////////////
//
//   Copyright (C) 86, 91, 1995-2002, Free Software Foundation, Inc.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2, or (at your option)
//   any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software Foundation,
//   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
//  
////////////////////////////////////////////////////////////////////////////
//
// Thanks to Amish S. Dave for getting us started up!
// Savagely hacked by Paul Knowles <Paul.Knowles@unifr.ch>,  May, 2003
// Many Thanks Paul, MAH
//
////////////////////////////////////////////////////////////////////////////
//
// Please email liunx@iotech with any bug reports questions or comments
//
////////////////////////////////////////////////////////////////////////////

//   ** This device driver requires kernel version  2.4.0 or greater


#include <linux/version.h>
#include <linux/kernel.h>       
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>

#include "iottypes.h"
#include "daqx.h"
#include "ddapi.h"
#include "device.h"

#define VENDOR_ID 0x1616

//MAH: 04.02.04 DaqBoard 1000 test
#define DEVICE_ID_2000 0x0409
#define DEVICE_ID_1000 0x045A

#define DAQ_REG_SIZE 0x1002
#define PLX_REG_SIZE 0x100
#define READBUFSIZE (DMAINPBUFSIZE)
#define WRITEBUFSIZE (DMAOUTBUFSIZE)


/* global data */
static DEVICE_EXTENSION *pdeArray[MAX_SUPPORTED_DEVICE];
static int bufsize = 8 * PAGE_SIZE;

//static PCI_SITE daqBoard[MAX_SUPPORTED_DEVICE];

/*
 *	pages_flag - set or clear a flag for sequence of pages
 *     (from https://github.com/makelinux/ldt.git)
 *
 *	more generic solution instead of SetPageReserved, ClearPageReserved etc
 *
 *	
 */

static void pages_flag(struct page *page, int page_num, int mask, int value)
{
	for (; page_num; page_num--, page++)
		if (value)
			__set_bit(mask, &page->flags);
		else
			__clear_bit(mask, &page->flags);
}


static int __init db2k_init(void);
static void db2k_cleanup(void);
static irqreturn_t __init db2k_irq(int irq, void *dev_id);
static int __init db2k_detect(int);
static int __init db2k_probe(void);
static int __init allocate_buffers(DEVICE_EXTENSION * pde);
typedef      int (*flush_t) (struct file *, fl_owner_t id);

static int db2k_open(struct inode *inode, struct file *file);
static int db2k_flush(struct file *filp, fl_owner_t id);
static int db2k_close(struct inode *inode, struct file *file);
static int db2k_mmap(struct file *flip, struct vm_area_struct *vma);
static long db2k_ioctl_check(unsigned int cmd, unsigned long arg);
static long db2k_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int proc_setup(void);
static ssize_t proc_2k0_type_read(struct file *filp,char *buf,size_t count,loff_t *offp);
static ssize_t proc_2k1_type_read(struct file *filp,char *buf,size_t count,loff_t *offp);
static ssize_t proc_2k2_type_read(struct file *filp,char *buf,size_t count,loff_t *offp);
static ssize_t proc_2k3_type_read(struct file *filp,char *buf,size_t count,loff_t *offp);
static int proc_2k0_type_open(struct inode *inode, struct file *file);
static int proc_2k1_type_open(struct inode *inode, struct file *file);
static int proc_2k2_type_open(struct inode *inode, struct file *file);
static int proc_2k3_type_open(struct inode *inode, struct file *file);

extern VOID entryIoctl(PDEVICE_EXTENSION pde, DWORD ioctlCode, daqIOTP pIn);

static struct file_operations db2k_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = db2k_ioctl,
    .mmap = db2k_mmap,
    .open = db2k_open,
    .flush = db2k_flush,
    .release = db2k_close,
};

static struct proc_dir_entry *iotech_proc_entry;
static struct proc_dir_entry *driver_proc_entry;
static struct proc_dir_entry *db2k_proc_entry;
static struct proc_dir_entry *db2K0_proc_entry;
static struct proc_dir_entry *db2K1_proc_entry;
static struct proc_dir_entry *db2K2_proc_entry;
static struct proc_dir_entry *db2K3_proc_entry;
static struct proc_dir_entry *type0_proc_entry;
static struct proc_dir_entry *type1_proc_entry;
static struct proc_dir_entry *type2_proc_entry;
static struct proc_dir_entry *type3_proc_entry;


static struct file_operations proc_fops0 = {
  read: proc_2k0_type_read,
  open: proc_2k0_type_open
};

static struct file_operations proc_fops1 = {
  read: proc_2k1_type_read,
  open: proc_2k1_type_open
};

static struct file_operations proc_fops2 = {
  read: proc_2k2_type_read,
  open: proc_2k2_type_open
};


static struct file_operations proc_fops3 = {
  read: proc_2k3_type_read,
  open: proc_2k3_type_open
};

// The following are for the purpose of signalling the end of /proc/.././type read streams.
static int proc2k0_reading = 0;
static int proc2k1_reading = 0;
static int proc2k2_reading = 0;
static int proc2k3_reading = 0;


/* Handle /proc/.../DaqBoard2k0/type open and read in the next two functions */

static int proc_2k0_type_open(struct inode *inode, struct file *file)
{
    proc2k0_reading = 0;
    return 0;
}

static ssize_t proc_2k0_type_read(struct file *filp,char *buf,size_t count,loff_t *offp)
{
    ssize_t len = 0;
    DEVICE_EXTENSION *pde = pdeArray[0];
    char msg[8]; // Our msg is only 1-3 bytes long.

    // We assume the maximum 3 bytes would be returned by the first call to the read
    // function and hence we can signal end of read in the second call to this
    // function without a call to the open function.
    if (proc2k0_reading == 1) {
	proc2k0_reading = 0;
	return 0;
    }

    proc2k0_reading = 1;

    try_module_get(THIS_MODULE);

    if(pde == NULL)
    {
	len = sprintf(msg, "%i\n",0);
    }
    else
    {
	len = sprintf(msg, "%i\n",pde->psite.deviceType);
    }        

    if(len > count)
    {
	len = count;
    }

    if  ( copy_to_user(buf, msg, len) != 0)  {
      info("proc_2k3_type_read:  copy_to_user did not copy all bytes");
      len =  -EFAULT;
    }

    module_put(THIS_MODULE);
    return len;
}



/* Handle /proc/.../DaqBoard2k1/type open and read in the next two functions */

static int proc_2k1_type_open(struct inode *inode, struct file *file)
{
    proc2k1_reading = 0;
    return 0;
}

static ssize_t proc_2k1_type_read(struct file *filp,char *buf,size_t count,loff_t *offp)
{
    ssize_t len = 0;
    DEVICE_EXTENSION *pde = pdeArray[1];
    char msg[8]; // Our msg is only 1-3 bytes long.

    // We assume the maximum 3 bytes would be returned by the first call to the read
    // function and hence we can signal end of read in the second call to this
    // function without a call to the open function.
    if (proc2k1_reading == 1) {
	proc2k1_reading = 0;
	return 0;
    }

    proc2k1_reading = 1;

    try_module_get(THIS_MODULE);

    if(pde == NULL)
    {
	len = sprintf(msg, "%i\n",0);
    }
    else
    {
	len = sprintf(msg, "%i\n",pde->psite.deviceType);
    }        

    if(len > count)
    {
	len = count;
    }

    if  ( copy_to_user(buf, msg, len) != 0)  {
      info("proc_2k3_type_read:  copy_to_user did not copy all bytes");
      len =  -EFAULT;
    }

    module_put(THIS_MODULE);
    return len;
}


/* Handle /proc/.../DaqBoard2k2/type open and read in the next two functions */

static int proc_2k2_type_open(struct inode *inode, struct file *file)
{
    proc2k2_reading = 0;
    return 0;
}

static ssize_t proc_2k2_type_read(struct file *filp,char *buf,size_t count,loff_t *offp)
{
    ssize_t len = 0;
    DEVICE_EXTENSION *pde = pdeArray[2];
    char msg[8]; // Our msg is only 1-3 bytes long.

    // We assume the maximum 3 bytes would be returned by the first call to the read
    // function and hence we can signal end of read in the second call to this
    // function without a call to the open function.
    if (proc2k2_reading == 1) {
	proc2k2_reading = 0;
	return 0;
    }

    proc2k2_reading = 1;

    try_module_get(THIS_MODULE);

    if(pde == NULL)
    {
	len = sprintf(msg, "%i\n",0);
    }
    else
    {
	len = sprintf(msg, "%i\n",pde->psite.deviceType);
    }        

    if(len > count)
    {
	len = count;
    }

    if  ( copy_to_user(buf, msg, len) != 0)  {
      info("proc_2k3_type_read:  copy_to_user did not copy all bytes");
      len =  -EFAULT;
    }

    module_put(THIS_MODULE);
    return len;
}


/* Handle /proc/.../DaqBoard2k3/type open and read in the next two functions */

static int proc_2k3_type_open(struct inode *inode, struct file *file)
{
    proc2k3_reading = 0;
    return 0;
}

static ssize_t proc_2k3_type_read(struct file *filp,char *buf,size_t count,loff_t *offp)
{
    ssize_t len = 0;
    DEVICE_EXTENSION *pde = pdeArray[3];
    char msg[8]; // Our msg is only 1-3 bytes long.

    // We assume the maximum 3 bytes would be returned by the first call to the read
    // function and hence we can signal end of read in the second call to this
    // function without a call to the open function.
    if (proc2k3_reading == 1) {
	proc2k3_reading = 0;
	return 0;
    }

    proc2k3_reading = 1;

    try_module_get(THIS_MODULE);

    if(pde == NULL)
    {
	len = sprintf(msg, "%i\n",0);
    }
    else
    {
	len = sprintf(msg, "%i\n",pde->psite.deviceType);
    }        

    if(len > count)
    {
	len = count;
    }

    if  ( copy_to_user(buf, msg, len) != 0)  {
      info("proc_2k3_type_read:  copy_to_user did not copy all bytes");
      len =  -EFAULT;
    }

    module_put(THIS_MODULE);
    return len;
}


static int __init
db2k_init(void)
{
    int i, result;
    int errstat         = 0;
    int num_db2k        = 0;
    int num_db1k        = 0;
    int total_boards    = 0;     

    DEVICE_EXTENSION *pde = NULL;

    for (i = 0; i< MAX_SUPPORTED_DEVICE; i++)
	pdeArray[i] = NULL;


    errstat = proc_setup();

    if(errstat)
    {
	info("Error setting up /proc file entries");
    }

    //MAH: 04.02.04 Search for 1000 and 2000 series boards
    info("Looking for 1000 series DaqBoards");
    num_db1k = db2k_detect(DEVICE_ID_1000);
    info("Found %i 1000 series DaqBoards%s.", num_db1k, (num_db1k>1 ? "s" : ""));

    info("Looking for 2000 series DaqBoards");
    num_db2k = db2k_detect(DEVICE_ID_2000);
    info("Found %i 2000 series DaqBoards%s.", num_db2k, (num_db2k>1 ? "s" : ""));

    total_boards = num_db1k + num_db2k;

    if (total_boards <= 0)
    {
	info("No DaqBoard1000/2000 series boards were found");
	return -(EIO);
    }    

    /* Get our major before stepping on any other toes */
    errstat = register_chrdev(DB2K_MAJOR, "db2k", &db2k_fops);
    if (errstat < 0)
    {
	info("Unable to get major %d for db2k device", (int) DB2K_MAJOR);
	goto out;
    }

    for (i = 0; i < total_boards; i++)
    {
	pde = (DEVICE_EXTENSION *) kmalloc(sizeof(DEVICE_EXTENSION), GFP_KERNEL);
	if (pde == NULL)
	{
	    errstat = -(ENOMEM);
	    goto out;
	}
	memset(pde, 0x00, sizeof (DEVICE_EXTENSION));
	pde->cleanupflags = 0;
	pde->user_count = 0;
	pde->id = i;
	spin_lock_init(& pde->open_lock);
	spin_lock_init(& pde->adcStatusMutex);
	spin_lock_init(& pde->dacStatusMutex);
	pdeArray[i] = pde;
	dbg("memory kmalloc'd for board %d", i);
    }

    if(db2k_probe())
    {
	errstat = -(ENOMEM);
	goto out;
    } 

    for (i = 0; i < total_boards; i++)
    {
	pde = pdeArray[i];
	if (check_mem_region(pde->daq_base_phys, DAQ_REG_SIZE) != 0)
	{
	    info("base registers 0x%lx to 0x%lx already in use",
		    pde->daq_base_phys, pde->daq_base_phys + DAQ_REG_SIZE - 1);
	    errstat = -(EBUSY);
	    goto out;
	}

	if (check_mem_region(pde->plx_base_phys, PLX_REG_SIZE) != 0)
	{
	    info("base registers 0x%lx to 0x%lx already in use",
		    pde->plx_base_phys, pde->plx_base_phys + PLX_REG_SIZE - 1);
	    errstat = -(EBUSY);
	    goto out;
	}
    }


    for (i = 0; i < total_boards; i++)
    {
	pde = pdeArray[i];
	/* we have checked the regions above
	 * so this should work with some confidence.
	 * better would be to check here also.
	 */
	request_mem_region(pde->daq_base_phys, DAQ_REG_SIZE, "db2k");
	request_mem_region(pde->plx_base_phys, PLX_REG_SIZE, "db2k");
	pde->cleanupflags |= REGIONS_REQUESTED;

	if (pde->irq > 0)
	{
	    result = request_irq(pde->irq, db2k_irq, IRQF_SHARED, "db2k", pde);

	    if (result)
	    {
		/* This i& DEVICE_ID_1000s not fatal, DaqBoards don't all have interrupts! */
		info("can't request IRQ %d", pde->irq);
	    }
	    else
	    {
		pde->cleanupflags |= IRQ_REQUESTED;
		info(" successfully obtained IRQ %d",pde->irq);
	    }
	}
    }

    if (errstat != 0)
	goto out;


    for (i = 0; (i < total_boards) && (errstat==0); i++)
    {
	pde = pdeArray[i];
	errstat = allocate_buffers(pde);
	if (errstat == 0)
	{
	    info("Successfully Installed driver db2k V0.1, MAJOR = %i", (int) DB2K_MAJOR);
	    /* somewhere from here on in there are SMP issues */
	    if (checkForEEPROM(pde))
	    {
		if (loadFpga(pde))
		{
		    getBoardInfo(pde);
		    if (initializeAdc(pde) && initializeDac(pde))
		    {
			pde->online = TRUE;
		    }
		    else
		    {
			errstat = -1;
		    }
		}
	    }
	}
    }


    /* cleanup to die with honour */
out:
    if (errstat != 0)
    {
	db2k_cleanup();
    }

    return errstat;
}


/* called during init to set up the read and write areas */
static int __init
allocate_buffers(DEVICE_EXTENSION * pde)
{

    int ret = 0;
    pde->readbuf = NULL;
    pde->writebuf = NULL;

    pde->readbuf = (short *) pci_alloc_consistent(pde->pdev, READBUFSIZE * sizeof(short),
	    & pde->readbuf_busaddr);
    if (!pde->readbuf)
    {
	ret =  -ENOMEM;
        goto exit;
    }
    pde->dmaInpBuf = pde->readbuf;
    pde->readbufsize = READBUFSIZE;
    pde->cleanupflags |= READBUF_ALLOCATED;
    dbg("readbuf %d bytes allocated @addr:  VIRT:0x%lx  BUS:0x%lx  PHYS:0x%lx",
	    pde->readbufsize, (ulong) pde->readbuf,
	    (ulong) virt_to_bus((void *) pde->readbuf),
	    (ulong) virt_to_phys((void *) pde->readbuf));


    pde->writebuf = (short *) pci_alloc_consistent(pde->pdev, WRITEBUFSIZE * sizeof(short),
	    & pde->writebuf_busaddr);
    if (!pde->writebuf)
    {
	ret  =   -ENOMEM;
        goto exit;
    }

    pde->dmaOutBuf = pde->writebuf;
    pde->writebufsize = WRITEBUFSIZE;
    pde->cleanupflags |= WRITEBUF_ALLOCATED;
    dbg("writebuf %d bytes allocated @addr:  VIRT:0x%lx  BUS:0x%lx  PHYS:0x%lx",
	    pde->writebufsize, (ulong) pde->writebuf,
	    (ulong) virt_to_bus((void *) pde->writebuf),
	    (ulong) virt_to_phys((void *) pde->writebuf));
	/*
	 *	Allocating buffers and pinning them to RAM
	 *	to be mapped to user space in db2k_mmap
	 */
	pde->in_buf = alloc_pages_exact(bufsize, GFP_KERNEL | __GFP_ZERO);
	if (!pde->in_buf) {
		ret = -ENOMEM;
		goto exit;
	}
	pages_flag(virt_to_page(pde->in_buf), PFN_UP(bufsize), PG_reserved, 1);
	pde->out_buf = alloc_pages_exact(bufsize, GFP_KERNEL | __GFP_ZERO);
	if (!pde->out_buf) {
		ret = -ENOMEM;
		goto exit;
	}
	pages_flag(virt_to_page(pde->out_buf), PFN_UP(bufsize), PG_reserved, 1);
exit:
        dbg("ret = %d", ret);
        if (ret < 0) {
          db2k_cleanup();
        }
    return ret;
}


struct device *Daq_dev = NULL;
static int __init
db2k_detect(int device_id_num)
{
    struct pci_dev *pdev = NULL;
    int ncards = 0;

    while ((pdev = pci_get_device(VENDOR_ID,device_id_num , pdev)))
    {
	ncards++;
        Daq_dev = &pdev->dev;   // use for dev_dbg() calls
    }	
    return ncards;
}


static int __init
db2k_probe(void)
{
    struct pci_dev *pdev = NULL;
    DEVICE_EXTENSION *pde = NULL;
    unsigned char pci_latency;
    unsigned char irqline, irqpin;
    unsigned int subid;
    unsigned int tempID = 0;
    int x, numBoards;
    int daqType[2] = {DEVICE_ID_1000, DEVICE_ID_2000};

    numBoards = 0;

    for (x = 0; x < 2; x++)
    {            
	//MAH: 04.02.04 Will not find DaqBoard 1000's    
	while ((pdev = pci_get_device(VENDOR_ID, daqType[x], pdev)) 
		&& (numBoards < MAX_SUPPORTED_DEVICE ) )
	{
	    pde = pdeArray[numBoards];
	    pde->pdev = pdev;

	    if (pci_enable_device(pdev))
	    {
		err("pci_enable_device failed.");
		/* skip this nonactive device and find the next one. 
		 * Nothing has, as yet, been allocated so 
		 * don't increment numBoards.
		 */
		continue;
	    }

	    pci_read_config_dword(pdev, PCI_SUBSYSTEM_VENDOR_ID, &subid);
	    tempID = subid;
	    pde->psite.protocol = DaqProtocolPCI;

	    switch (daqType[x])
	    {
		case DEVICE_ID_2000:
		    {
			switch (tempID)
			{
			    case DAQBOARD2000_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard2000;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD2000");
				break;

			    case DAQBOARD2001_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard2001;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD2001");
				break;

			    case DAQBOARD2002_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard2002;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD2002");
				break;

			    case DAQBOARD2003_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard2003;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD2003");
				break;

			    case DAQBOARD2004_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard2004;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD2004");
				break;

			    case DAQBOARD2005_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard2005;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD2005");
				break;

			    default:
				pde->psite.deviceType = DaqBoard2000;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "Unknown");
				break;
			}                    
		    }
		    break;

		case DEVICE_ID_1000:                
		    {
			switch (tempID)
			{
			    //MAH: 04.02.04 Two new cases for 1000 series
			    case DAQBOARD1000_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard1000;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD1000");
				break;

			    case DAQBOARD1005_SUBSYSTEM_IDS:
				pde->psite.deviceType = DaqBoard1005;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "DAQBOARD1005");
				break;

			    default:
				pde->psite.deviceType = DaqBoard1000;
				info("board %d, vendor_id: %8.8x deviceType: %d (%s)",
					numBoards, tempID, pde->psite.deviceType, "Unknown");
				break;
			}                    
		    }
		    break;
	    }//end switch (daqType[x])

	    irqline = 0;
	    irqpin = 0;

	    pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &irqline);
	    pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &irqpin);

	    if ((irqline == 0) || (irqline > 15))
	    {
		irqline = 0;
		info("pci irqs not supported by card #%d", numBoards);
	    }
	    else
	    {
		info("db2k pdev->irq %d, irq line %d, pin %d",
			pdev->irq, irqline, irqpin);
	    }

	    pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &pci_latency);

	    if (pci_latency <= 0x58)
	    {
		info("pci latency=0x%02x, resetting to 0x58", pci_latency);
		pci_latency = 0x58;
		pci_write_config_byte(pdev, PCI_LATENCY_TIMER, pci_latency);
	    }

	    /* need to delimit the address space this puppy can use
	     * i386 supports 24 bit PCI addresses... the board uses 
	     * a PLX-9080-3, which should do 32 bit addresses?
	     * what do we really nead here?
	     */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,17)
	    if ((pci_set_dma_mask(pdev, (dma_addr_t) 0x00ffffff)) == -EIO)
	    {
		/* dma at 24 bits not supported by the kernel 
		 * the error handling here is not immediately clear for me...
		 */
		pde->cleanupflags |= DMA_ADDRESS_PROBLEM;
		err("**HELP** DMA not supported by kernel: PEK:FIXME");			
	    }
#endif

	    pci_set_master(pdev);

	    pde->irq = irqline;
	    pde->plx_base_phys = pci_resource_start(pdev, 0);
	    pde->daq_base_phys = pci_resource_start(pdev, 2);
	    pde->plx_base = ioremap(pde->plx_base_phys, PLX_REG_SIZE);
	    pde->daq_base = ioremap(pde->daq_base_phys, DAQ_REG_SIZE);
	    info("Registers remapped.");
	    pde->nints = 0;
	    pde->cleanupflags |= REGS_REMAPPED;

	    pde->daqBaseAddress = pde->daq_base;
	    pde->plxVirtualAddress = pde->plx_base;
	    numBoards++;

	}//end while
    }//end for
    return 0;
}

/*
 * All cleanup functions happen here, no matter where the error occured.
 * the cleanupflags keeps the state for each device, and on any error we 
 * arrive here, frantically hoping that cleanupflags isn't corrupt (in which
 * case we are hopelessly buggered anyway: all the addresses will be skanky too!)
 */
static void 
db2k_cleanup(void)
{
    int i;
    DEVICE_EXTENSION *pde = NULL;

    for (i = MAX_SUPPORTED_DEVICE-1; i > -1; i--) {
	pde = pdeArray[i];
	if (pde == NULL) {
	    continue;
	}
	info("Releasing board %d", i);
	if (pde->in_buf) {
		pages_flag(virt_to_page(pde->in_buf), PFN_UP(bufsize), PG_reserved, 0);
		free_pages_exact(pde->in_buf, bufsize);
	}
	if (pde->out_buf) {
		pages_flag(virt_to_page(pde->out_buf), PFN_UP(bufsize), PG_reserved, 0);
		free_pages_exact(pde->out_buf, bufsize);
	}

	/* clean up in the inverse order of allocation */
	if (pde->cleanupflags & WRITEBUF_ALLOCATED) {
	    dbg("write buffers to release.");
	    if (pde->writebuf) {
		pci_free_consistent(pde->pdev, WRITEBUFSIZE * sizeof(short),
			pde->writebuf, pde->writebuf_busaddr);
		pde->writebuf = NULL;
		dbg("Released DMA write buffer");
	    }

	    if (pde->cleanupflags & DACTIMER_ADDED) {
		dbg("Cleaning dactimer. PEK:FIXME");
		del_timer(&pde->dac_timer);
	    }

	    if (pde->cleanupflags & ADCTIMER_ADDED) {
		dbg("Cleaning adctimer. PEK:FIXME");
		del_timer(&pde->adc_timer);
	    }

	    if (pde->cleanupflags & DMACHAIN0_ALLOCATED) {
		if (pde->dma0_chain) {
		    //					kfree(pde->dma0_chain);
		    dbg("Cleaning up DMAchain0. PEK:FIXME");
		}
	    }

	    if (pde->cleanupflags & DMACHAIN1_ALLOCATED) {
		if (pde->dma1_chain) {
		    //					kfree(pde->dma1_chain);
		    dbg("Cleaning up DMAchain1. PEK:FIXME");
		}
	    }
	}

	if (pde->cleanupflags & READBUF_ALLOCATED) {
	    dbg("Read buffers to release.");
	    if (pde->readbuf) {
		pci_free_consistent(pde->pdev, READBUFSIZE * sizeof(short),
			pde->readbuf, pde->readbuf_busaddr);
		dbg("Released DMA read buffer.");
	    }
	    pde->readbuf = NULL;
	}


	if (pde->cleanupflags & REGS_REMAPPED) {
	    iounmap((void *) pde->daq_base);
	    iounmap((void *) pde->plx_base);
	    dbg("Released register maps.");
	}

	if (pde->cleanupflags & IRQ_REQUESTED) {
	    free_irq(pde->irq, pde);
	}

	if (pde->cleanupflags & REGIONS_REQUESTED) {
	    release_mem_region(pde->daq_base_phys, DAQ_REG_SIZE);
	    release_mem_region(pde->plx_base_phys, PLX_REG_SIZE);
	    dbg("Released IO memory.");
	}

	kfree(pde);
	pdeArray[i] = NULL;
	dbg("memory kfree'd for board %d.",i);
    }

    unregister_chrdev(DB2K_MAJOR, "db2k");
    info("All boards released, unloading.");

    //Cleanup the mess you made in the proc_fs
    remove_proc_entry("type",db2K0_proc_entry);
    remove_proc_entry("type",db2K1_proc_entry);
    remove_proc_entry("type",db2K2_proc_entry);
    remove_proc_entry("type",db2K3_proc_entry);
    remove_proc_entry("DaqBoard2K0",db2k_proc_entry);
    remove_proc_entry("DaqBoard2K1",db2k_proc_entry);
    remove_proc_entry("DaqBoard2K2",db2k_proc_entry);
    remove_proc_entry("DaqBoard2K3",db2k_proc_entry);
    remove_proc_entry("db2k",driver_proc_entry);
    remove_proc_entry("drivers",iotech_proc_entry);
    remove_proc_entry("iotech",NULL);

    return;
}

static int
proc_setup(void)
{
    info("Setting up iotech /proc file entries");

    //MAH: Due to a chicken/egg issue in the api, proc file entries
    //are created to hold device and driver info. Although most are
    //currently unused, I wanted to leave room for further expansion
    iotech_proc_entry = proc_mkdir("iotech",NULL);
    driver_proc_entry = proc_mkdir("drivers",iotech_proc_entry);
    db2k_proc_entry =   proc_mkdir("db2k",driver_proc_entry);
    db2K0_proc_entry =  proc_mkdir("DaqBoard2K0",db2k_proc_entry);
    db2K1_proc_entry =  proc_mkdir("DaqBoard2K1",db2k_proc_entry);
    db2K2_proc_entry =  proc_mkdir("DaqBoard2K2",db2k_proc_entry);
    db2K3_proc_entry =  proc_mkdir("DaqBoard2K3",db2k_proc_entry);


    type0_proc_entry = proc_create("type",0444,db2K0_proc_entry,&proc_fops0);

    type1_proc_entry = proc_create("type",0444,db2K1_proc_entry,&proc_fops1);

    type2_proc_entry = proc_create("type",0444,db2K2_proc_entry,&proc_fops2);

    type3_proc_entry = proc_create("type",0444,db2K3_proc_entry,&proc_fops3);

    return 0;
}


/*
 * fops functions from here on down...
 */

static int
db2k_open(struct inode *inode, struct file *file)
{
    int minor = MINOR(inode->i_rdev);
    int stat = 0;
    DEVICE_EXTENSION *pde = NULL;
    dbg("db2k_open called");

    if (minor >= MAX_SUPPORTED_DEVICE) {
	return -(ENODEV);
    }

    if ((pde = pdeArray[minor]) == NULL) {
	return -(ENODEV);
    }

    /* all access to pde->user_count during the open method
     * must be protected by a spin_lock 
     */
    spin_lock(&pde->open_lock);
    {
	if(pde->user_count){
	    spin_unlock(&pde->open_lock);
	    return -EBUSY;
	}

	file->private_data = pde;
	pde->user_count++;
	file->f_op = &db2k_fops;
    }
    spin_unlock(&pde->open_lock);

    info("open major/minor = %d/%d", DB2K_MAJOR, minor);

    try_module_get(THIS_MODULE);

    return stat;
}

static int
db2k_flush(struct file *filp, fl_owner_t id)
{
	int minor = MINOR(filp->f_dentry->d_inode->i_rdev);

	DEVICE_EXTENSION *pde = NULL;
	pde = pdeArray[minor];

	pde->user_count--;
	//printk("CLOSING UP THIS INSTANCE user count is %i\n",pde->user_count); 
	adcDisarm(pde, NULL);
	dacDisarm(pde, NULL);
	adcStop(pde, NULL);
	adcStop(pde, NULL);
	info("CLOSED DRIVER HANDLE");

	return 0;

}


static int
db2k_close(struct inode *inode, struct file *file)
{
/*
	DEVICE_EXTENSION *pde = (DEVICE_EXTENSION *) file->private_data;
	pde->user_count--;

    // MAH: 06.24.03 from Paul - Shut it all down!
    adcDisarm(pde, NULL);
    dacDisarm(pde, NULL);
    adcStop(pde, NULL);
    adcStop(pde, NULL);
*/
    
	module_put(THIS_MODULE);

	return 0;
}


static irqreturn_t
db2k_irq(int irq, void *dev_id)
{
    DEVICE_EXTENSION *pde;
    int dma0Status, dma1Status;

    if (dev_id == NULL) {
	/* spurious interrupt error? */
	return IRQ_NONE;
    }

    /* we are board pde->id */
    pde = (DEVICE_EXTENSION *) dev_id;

    dma0Status = readb(pde->plx_base + PCI9080_DMA0_CMD_STATUS);
    dma1Status = readb(pde->plx_base + PCI9080_DMA1_CMD_STATUS);

    dbg("interrupt on board %i: irq->dma0 %x, dma1 %x",
	    pde->id, dma0Status, dma1Status);

    if (pde->dmaInpActive) {
	dbg("adc interrupt not serviced");
    }
    return IRQ_HANDLED;
}


//MAH  re-implemet this properly!
static long
db2k_ioctl_check(unsigned int cmd, unsigned long arg)
{ 
	return 0;
}


static long
db2k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
    int stat = 0;
    DEVICE_EXTENSION *pde = NULL;
    daqIOTP p =  NULL ;
    void __user *user = (void __user *) arg;

    if (minor >= MAX_SUPPORTED_DEVICE)
    {
        //info("db2k_ioctl return1: %d", -(ENODEV));
	return -(ENODEV);
    }

    if ((pde = pdeArray[minor]) == NULL)
    {
        //info("db2k_ioctl return2: %d", -(ENODEV));
	return -(ENODEV);
    }
    p = (daqIOTP ) (pde->in_buf); // this buffer allocated by alloc_pages_exact(bufsize, GFP_KERNEL | __GFP_ZERO)

    if ((stat = db2k_ioctl_check(cmd, arg)) != 0)
    {
        //info("db2k_ioctl return3: %d", stat);
	err("ioctl_check FAILED, command %i", cmd);
	return stat;
    }

    // copy entire daqIOT structure from user space to kernel space buffer 'p'
    if (copy_from_user(p, user, sizeof (daqIOT)) != 0) {
      info("db2k_ioctl : copy_from_user did not copy all bytes");
      return -EFAULT;
    }


    if (pde->online == TRUE)
    {
	entryIoctl(pde, cmd, p);
        //info("db2k_ioctl return4: %d", ERROR_SUCCESS);
        stat = ERROR_SUCCESS;
    } else {
        p->errorCode = DerrNotOnLine; 
        info("Board %i not online.", pde->id);
        stat = -EINVAL;
    }
    // copy entire daqIOT structure back to user space
    if (copy_to_user(user, p,  sizeof (daqIOT)) != 0) {
      info("db2k_ioctl : copy_from_user did not copy all bytes");
      stat =  -EFAULT;
    }


    //info("db2k_ioctl return5: %d", stat);
    return stat;
}

#ifdef NOTDEF

static void
db2k_mmap_open(struct vm_area_struct *vma)
{
	try_module_get(THIS_MODULE); 

	return ;
}

/*
 * My understanding of mmap methods is not perfect, but I think this is wrong here.
 * The vm_operations_struct->open function is called across fork(); which is fine
 * for the open above.  So our user space forks and the usage count is incremented
 * then one forked process exits, and close is called: poof the mmap disappears
 * leaving to other process a little bit, well, forked...
 * /proc/PID/maps is traversed at process end, freeing the maps it held.
 * we shouldn't have to do it explicitly.
 *
 * besides, keeping the global *buffer around just to do this is ugly.
 * hint: uglyness*hardness = Best_Measure_of(wrongness)
 */
static void
db2k_mmap_close(struct vm_area_struct *vma)
{
	module_put(THIS_MODULE); 

//	struct page *page;
//	unsigned long size = (unsigned long) (vma->vm_end - vma->vm_start);

//	for (page = virt_to_page(buffer); 
//	     page < virt_to_page(buffer + size); page++) {
//		mem_map_unreserve(page);
//	}
//	free_pages((unsigned long) buffer, gfp_size);
//	dbg("Releasing User Buffer");
	return ;
}


static struct vm_operations_struct db2k_vma_ops = {
	open:db2k_mmap_open,
	close:db2k_mmap_close,
};
#endif


static int
db2k_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int minor = MINOR(filp->f_dentry->d_inode->i_rdev);
    void *buf = NULL;

    DEVICE_EXTENSION *pde = NULL;

    if (minor >= MAX_SUPPORTED_DEVICE)
    {
	return -(ENODEV);
    }

    if ((pde = pdeArray[minor]) == NULL)
    {
	return -(ENODEV);
    }

	if (vma->vm_flags & VM_WRITE)
		buf = pde->in_buf;
	else if (vma->vm_flags & VM_READ)
		buf = pde->out_buf;
	if (!buf)
		return -EINVAL;
	if (remap_pfn_range(vma, vma->vm_start, virt_to_phys(buf) >> PAGE_SHIFT,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		pr_err("%s\n", "remap_pfn_range failed");
		return -EAGAIN;
	}
	return 0;
}

module_init(db2k_init);
module_exit(db2k_cleanup);


MODULE_AUTHOR("IOtech and others");
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,17)
MODULE_DESCRIPTION("Driver for IOtech DaqBoards");
MODULE_LICENSE("GPL");
#endif
