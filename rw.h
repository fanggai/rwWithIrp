//
// rw.h
//

NTSTATUS 
 LogReadFileWithIrp(
    IN PFILE_OBJECT fileObject, 
	IN OUT PCHAR buffer, 
	IN ULONG length, 
	IN PLARGE_INTEGER byteOffset);
	
NTSTATUS LogWriteFileWithIrp(
	IN PFILE_OBJECT fileObject, 
	IN PCHAR buffer, 
	IN ULONG length, 
	IN PLARGE_INTEGER byteOffset);
	
NTSTATUS LogGetFileSizeWithIrp(
	IN PFILE_OBJECT fileObject, 
	OUT PLARGE_INTEGER fileSize);
