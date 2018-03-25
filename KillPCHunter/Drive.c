/*
	ʵ�ֹرս��̺���,ɱ��pchunter
	https://bbs.pediy.com/thread-225434.htm
ԭ��
	����TerminateProcess��ʵ�֣��Լ���ʵ��һ��TerminateProcess
*/

#include <KrTypeDef.h>
#include <ntddk.h>


//��Ҫɱ���Ľ���id
#define PCHUNTER_ID   3956

VOID DriverUnload(PDRIVER_OBJECT pDriver);
PEPROCESS LookupProcess(HANDLE hPid);
PETHREAD LookupThread(HANDLE hTid);
VOID KillProcess(PEPROCESS pEProcess);
ULONG GetPspTerminateThreadByPointer();
ULONG GetPspExitThread(ULONG PspTerminateThreadByPointer);
VOID SelfTerminateThread(
	KAPC *Apc,
	PKNORMAL_ROUTINE *NormalRoutine,
	PVOID *NormalContext,
	PVOID *SystemArgument1,
	PVOID *SystemArgument2);

fpTypePspExitThread g_fpPspExitThreadAddr = NULL;

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pPath)
{
#ifdef _DEBUG
	DbgBreakPoint();
#endif // _DEBUG	
	pDriver->DriverUnload = DriverUnload;

	//��ǰ�Ѻ������ҳ���
	ULONG uPspTerminateThreadByPointerAddr = GetPspTerminateThreadByPointer();
	if (0 == uPspTerminateThreadByPointerAddr)
	{
		KdPrint(("����PspTerminateThreadByPointerAddr��ַ����\n"));
		return STATUS_SUCCESS;
	}
	g_fpPspExitThreadAddr = (fpTypePspExitThread)GetPspExitThread(uPspTerminateThreadByPointerAddr);
	if (NULL == g_fpPspExitThreadAddr)
	{
		KdPrint(("����PspExitThread��ַ����\n"));
		return STATUS_SUCCESS;
	}

	//
	PEPROCESS pProcess = LookupProcess((HANDLE)PCHUNTER_ID);
	if (NULL == pProcess)
	{
		KdPrint((("û����PsCidTable���ҵ�����,���겻�������˰�\n")));
	}
	else
	{
		KillProcess(pProcess);
	}

	return STATUS_SUCCESS;
}


VOID DriverUnload(PDRIVER_OBJECT pDriver)
{
	KdPrint(("�����˳�\n"));
}

PEPROCESS LookupProcess(HANDLE hPid)
{
	PEPROCESS pEProcess = NULL;
	if (NT_SUCCESS(PsLookupProcessByProcessId(hPid, &pEProcess)))
		return pEProcess;
	return NULL;
}

VOID KillProcess(PEPROCESS pEProcess)
{
	PEPROCESS pEProc = NULL;
	PETHREAD  pEThrd = NULL;
	ULONG i = 0;

	for (i = 4; i < 0x25600; i += 4) // �������е��̣߳���ȡ�߳����ڵĽ��̣��ж��Ƿ�ƥ�䡣Ŀ��֪ͨÿ���߳̽�����
	{
		pEThrd = LookupThread((HANDLE)i);
		if (!pEThrd)  continue;
		pEProc = IoThreadToProcess(pEThrd);
		if (pEProc == pEProcess)
		{
			PKAPC pApc = NULL;
			pApc = (PKAPC)ExAllocatePool(NonPagedPool, sizeof(KAPC));
			if (NULL == pApc) return;
			//�����ں�apc
			KeInitializeApc(pApc, (PKTHREAD)pEThrd, OriginalApcEnvironment, (PKKERNEL_ROUTINE)&SelfTerminateThread, NULL, NULL, 0, NULL);
			KeInsertQueueApc(pApc, NULL, 0, 2);

		}
		ObDereferenceObject(pEThrd);
	}
}

PETHREAD LookupThread(HANDLE hTid)
{
	PETHREAD pEThread = NULL;
	if (NT_SUCCESS(PsLookupThreadByThreadId(hTid, &pEThread)))
		return pEThread;
	return NULL;
}

VOID SelfTerminateThread(
	KAPC *Apc,
	PKNORMAL_ROUTINE *NormalRoutine,
	PVOID *NormalContext,
	PVOID *SystemArgument1,
	PVOID *SystemArgument2)
{
	ExFreePool(Apc);
	g_fpPspExitThreadAddr(STATUS_SUCCESS);
}

ULONG GetPspTerminateThreadByPointer()
{
	UNICODE_STRING funcName;
	RtlInitUnicodeString(&funcName, L"PsTerminateSystemThread");
	ULONG step = 0;
	ULONG targetFunAddr = 0;
	ULONG baseFunAddr = (ULONG)MmGetSystemRoutineAddress(&funcName);
	for (step = baseFunAddr; step < (baseFunAddr + 1024); step++)
	{
		//searching for 0x50,0xe8
		if (((*(PUCHAR)(UCHAR*)(step - 1)) == 0x50) && ((*(PUCHAR)(UCHAR*)(step)) == 0xe8))
		{
			ULONG offset = *(PULONG)(step + 1); // ȡ��ַ����ֽ���
			targetFunAddr = step + 5 + offset;
			break;
		}
	}
	return targetFunAddr;
} //PspExitThread stamp code:0x0c 0xe8

ULONG GetPspExitThread(ULONG PspTerminateThreadByPointer)
{
	ULONG step = 0;
	ULONG targetFunAddr = 0;
	ULONG baseFunc = PspTerminateThreadByPointer;
	for (step = baseFunc; step < (baseFunc + 1024); step++)
	{
		//searching for 0x0c,0xe8
		if (((*(PUCHAR)(UCHAR*)(step - 1)) == 0x0c) && ((*(PUCHAR)(UCHAR*)(step)) == 0xe8))
		{
			ULONG m_offset = *(PULONG)(step + 1);
			targetFunAddr = step + 5 + m_offset;
			break;
		}
	}
	return targetFunAddr;
}


