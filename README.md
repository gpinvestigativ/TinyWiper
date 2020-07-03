# TeensyWiper

The TeensyWiper is a small, battery powered device which securely deletes your SD cards by overwriting the whole card with randomly generated data.  

> *Note:* this is an successor project to the [SDCardWiper](https://github.com/gpinvestigativ/SDCardWiper) Project, which is based on a Raspberry Pi Zero and readily available parts.  

**Did you know?** Just formating your SD card does not delete its contents, it just makes them invisible within the filebrowser of your operating system. Special forensic software can often easily reconstruct data on deleted or formated SD cards.  

If you want to make sure that your data can not be reconstructed, this device will securely erase your SD cards.

## Hardware

add hardware description here

## Software

add software description here

## Q&A

*Q: why not use a secure erase software like the security options within macs disk manager?*  
A: While it is totaly viable to use secure erase software options for this task, the TeensyWiper has 2 major advantages:
* overwriting a 64GB SD card can take between 1-2 hours, 400GB multiple hours, if you don't want your computers SD port to be blocked for such a long time or don't like to have to keep your computer running, the TeensyWiper is a handy device which fits in your pocket.
* software that runs on your computer can much easier be infiltrated by a virus or an attacker. The TeensyWiper is an unteathered device, which makes it impossible to hack without physical access to it.