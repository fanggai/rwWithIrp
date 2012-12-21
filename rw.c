//
// rw.c
//

NTSTATUS
LogIoCompletionRoutine(
    IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context);
	
NTSTATUS LogIrpReadFile(
	IN  PFILE_OBJECT  FileObject,
	OUT PIO_STATUS_BLOCK  IoStatusBlock,
	OUT PVOID  Buffer,
	IN  ULONG  Length,
	IN  PLARGE_INTEGER  ByteOffset  OPTIONAL);
	
NTSTATUS
LogIrpWriteFile(
	IN  PFILE_OBJECT  FileObject,
	OUT PIO_STATUS_BLOCK  IoStatusBlock,
	OUT PVOID  Buffer,
	IN  ULONG  Length,
	IN  PLARGE_INTEGER  ByteOffset  OPTIONAL);

NTSTATUS LogReadFileWithIrp(PFILE_OBJECT fileObject, PCHAR buffer, ULONG length, PLARGE_INTEGER byteOffset)
{
	NTSTATUS status;
	IO_STATUS_BLOCK             IoStatusBlock;

	status = LogIrpReadFile(fileObject, &IoStatusBlock, buffer, length, byteOffset);

	return status;
}

NTSTATUS LogWriteFileWithIrp(PFILE_OBJECT fileObject, PCHAR buffer, ULONG length, PLARGE_INTEGER byteOffset)
{
	NTSTATUS status;
	IO_STATUS_BLOCK             IoStatusBlock;

	status = LogIrpWriteFile(fileObject, &IoStatusBlock, buffer, length, byteOffset);

	return status;
}

NTSTATUS LogGetFileSizeWithIrp(IN PFILE_OBJECT fileObject, OUT PLARGE_INTEGER fileSize)
{
	NTSTATUS status;
	IO_STATUS_BLOCK IoStatusBlock;
	FILE_INFORMATION_CLASS fileInformationClass = FileStandardInformation;
	FILE_STANDARD_INFORMATION fileStdInfo = {0};

	ASSERT( fileSize != NULL );

	status = LogQueryFileInfoWithIrp(
					 fileObject,
					 &IoStatusBlock,
					 fileInformationClass,
					 &fileStdInfo,
					 sizeof(FILE_STANDARD_INFORMATION)
					 );
	if( NT_SUCCESS(status) ) {
		fileSize->QuadPart = fileStdInfo.EndOfFile.QuadPart;
		status = STATUS_SUCCESS;
	}

	return status;
}

NTSTATUS
LogIoCompletionRoutine(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PVOID Context)
{
	*Irp->UserIosb = Irp->IoStatus;
	if (Irp->UserEvent)
		KeSetEvent(Irp->UserEvent, IO_NO_INCREMENT, 0);
	if (Irp->MdlAddress)
	{
		IoFreeMdl(Irp->MdlAddress);
		Irp->MdlAddress = NULL;
	}
	IoFreeIrp(Irp);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
LogIrpWriteFile(
	IN  PFILE_OBJECT  FileObject,
	OUT PIO_STATUS_BLOCK  IoStatusBlock,
	OUT PVOID  Buffer,
	IN  ULONG  Length,
	IN  PLARGE_INTEGER  ByteOffset  OPTIONAL)
{
	NTSTATUS ntStatus;
	PIRP     Irp;
	KEVENT   kEvent;
	PIO_STACK_LOCATION IrpSp;
 
	if (FileObject->Vpb == 0 || FileObject->Vpb->DeviceObject == NULL)
		return STATUS_UNSUCCESSFUL;
 
	if(ByteOffset == NULL)
	{
		if(!(FileObject->Flags & FO_SYNCHRONOUS_IO))
			return STATUS_INVALID_PARAMETER;
		ByteOffset = &FileObject->CurrentByteOffset;
	}
 
	Irp = IoAllocateIrp(FileObject->Vpb->DeviceObject->StackSize, FALSE);
	if(Irp == NULL) return STATUS_INSUFFICIENT_RESOURCES;
 
	if(FileObject->DeviceObject->Flags & DO_BUFFERED_IO)
	{
		Irp->AssociatedIrp.SystemBuffer = Buffer;
	}
	else if(FileObject->DeviceObject->Flags & DO_DIRECT_IO)
	{
		Irp->MdlAddress = IoAllocateMdl(Buffer, Length, 0, 0, 0);
		if (Irp->MdlAddress == NULL)
		{
			IoFreeIrp(Irp);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		MmBuildMdlForNonPagedPool(Irp->MdlAddress);
	}
	else
	{
		Irp->UserBuffer = Buffer;
	}
 
	KeInitializeEvent(&kEvent, SynchronizationEvent, FALSE);
 
	Irp->UserEvent     = &kEvent;
	Irp->UserIosb      = IoStatusBlock;
	Irp->RequestorMode = KernelMode;
	Irp->Flags         = IRP_WRITE_OPERATION;
	Irp->Tail.Overlay.Thread = PsGetCurrentThread();
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
 
	IrpSp = IoGetNextIrpStackLocation(Irp);
	IrpSp->MajorFunction = IRP_MJ_WRITE;
	IrpSp->MinorFunction = IRP_MN_NORMAL;
	IrpSp->DeviceObject = FileObject->Vpb->DeviceObject;
	IrpSp->FileObject = FileObject;
	IrpSp->Parameters.Write.Length = Length;
	IrpSp->Parameters.Write.ByteOffset = *ByteOffset;
 
	IoSetCompletionRoutine(Irp, LogIoCompletionRoutine, 0, TRUE, TRUE, TRUE);
	ntStatus = IoCallDriver(FileObject->Vpb->DeviceObject, Irp);
	if (ntStatus == STATUS_PENDING)
		KeWaitForSingleObject(&kEvent, Executive, KernelMode, TRUE, 0);
 
	return IoStatusBlock->Status;
}

NTSTATUS
LogIrpReadFile(
	IN  PFILE_OBJECT  FileObject,
	OUT PIO_STATUS_BLOCK  IoStatusBlock,
	OUT PVOID  Buffer,
	IN  ULONG  Length,
	IN  PLARGE_INTEGER  ByteOffset  OPTIONAL)
{
	NTSTATUS ntStatus;
	PIRP     Irp;
	KEVENT   kEvent;
	PIO_STACK_LOCATION IrpSp;
 
	if (FileObject->Vpb == 0 || FileObject->Vpb->DeviceObject == NULL)
		return STATUS_UNSUCCESSFUL;
 
	if(ByteOffset == NULL)
	{
		if(!(FileObject->Flags & FO_SYNCHRONOUS_IO))
			return STATUS_INVALID_PARAMETER;
		ByteOffset = &FileObject->CurrentByteOffset;
	}
 
	Irp = IoAllocateIrp(FileObject->Vpb->DeviceObject->StackSize, FALSE);
	if(Irp == NULL) return STATUS_INSUFFICIENT_RESOURCES;
 
	RtlZeroMemory(Buffer, Length);
	if(FileObject->DeviceObject->Flags & DO_BUFFERED_IO)
	{
		Irp->AssociatedIrp.SystemBuffer = Buffer;
	}
	else if(FileObject->DeviceObject->Flags & DO_DIRECT_IO)
	{
		Irp->MdlAddress = IoAllocateMdl(Buffer, Length, 0, 0, 0);
		if (Irp->MdlAddress == NULL)
		{
			IoFreeIrp(Irp);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		MmBuildMdlForNonPagedPool(Irp->MdlAddress);
	}
	else
	{
		Irp->UserBuffer = Buffer;
	}
 
	KeInitializeEvent(&kEvent, SynchronizationEvent, FALSE);
 
	Irp->UserEvent     = &kEvent;
	Irp->UserIosb      = IoStatusBlock;
	Irp->RequestorMode = KernelMode;
	Irp->Flags         = IRP_READ_OPERATION;
	Irp->Tail.Overlay.Thread = PsGetCurrentThread();
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
 
	IrpSp = IoGetNextIrpStackLocation(Irp);
	IrpSp->MajorFunction = IRP_MJ_READ;
	IrpSp->MinorFunction = IRP_MN_NORMAL;
	IrpSp->DeviceObject = FileObject->Vpb->DeviceObject;
	IrpSp->FileObject = FileObject;
	IrpSp->Parameters.Read.Length = Length;
	IrpSp->Parameters.Read.ByteOffset = *ByteOffset;
 
	IoSetCompletionRoutine(Irp, LogIoCompletionRoutine, 0, TRUE, TRUE, TRUE);
	ntStatus = IoCallDriver(FileObject->Vpb->DeviceObject, Irp);
	if (ntStatus == STATUS_PENDING)
		KeWaitForSingleObject(&kEvent, Executive, KernelMode, TRUE, 0);
 
	return IoStatusBlock->Status;
}

NTSTATUS
LogQueryFileInfoWithIrp(
					 PFILE_OBJECT FileObject,
					 OUT PIO_STATUS_BLOCK  IoStatusBlock,
					 FILE_INFORMATION_CLASS FileInformationClass,
					 PVOID FileInfo,
					 ULONG FileInfoLength
					 )
{
	NTSTATUS ntStatus;
	PIRP Irp;
	KEVENT kEvent;
	PIO_STACK_LOCATION IoStackLocation;

	if (FileObject->Vpb == 0 || FileObject->Vpb->DeviceObject == NULL)
		return STATUS_UNSUCCESSFUL;

	//
	// Initialize the kEvent
	//
	KeInitializeEvent(&kEvent, NotificationEvent, FALSE);

	Irp = IoAllocateIrp(FileObject->Vpb->DeviceObject->StackSize, FALSE);
	if(Irp == NULL) return STATUS_INSUFFICIENT_RESOURCES;

	Irp->AssociatedIrp.SystemBuffer = FileInfo;
	Irp->UserEvent = &kEvent;
	Irp->UserIosb = IoStatusBlock;
	Irp->Tail.Overlay.Thread = PsGetCurrentThread();
	Irp->Tail.Overlay.OriginalFileObject = FileObject;
	Irp->RequestorMode = KernelMode;
	Irp->Flags = 0;

	IoStackLocation = IoGetNextIrpStackLocation(Irp);
	IoStackLocation->MajorFunction = IRP_MJ_QUERY_INFORMATION;
	IoStackLocation->DeviceObject = FileObject->Vpb->DeviceObject;
	IoStackLocation->FileObject = FileObject;
	IoStackLocation->Parameters.QueryFile.Length = FileInfoLength;
	IoStackLocation->Parameters.QueryFile.FileInformationClass = FileInformationClass;

	IoSetCompletionRoutine(Irp, LogIoCompletionRoutine, 0, TRUE, TRUE, TRUE);

	ntStatus = IoCallDriver(FileObject->Vpb->DeviceObject, Irp);

	if (ntStatus == STATUS_PENDING)
		KeWaitForSingleObject(&kEvent, Executive, KernelMode, TRUE, 0);

	return IoStatusBlock->Status;
}
