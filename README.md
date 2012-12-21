rwWithIrp
=========

Code fragments to read, write, and get file size using your irp in kernel mode driver.



Implementation: 
Here we use a file object pointer and a made-up irp to read/write/getFileInfo 
instead of using ZwReadFile()/ZwWriteFile()/ZwQueryFileInformation().

Test:  
Have tested on Windows xp sp2. Should work well on windows 7.
