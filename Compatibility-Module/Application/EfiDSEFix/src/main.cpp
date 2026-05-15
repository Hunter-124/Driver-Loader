#include "EfiDSEFix.h"
#include <ntstatus.h>

static
VOID
PrintUsage(
	_In_ PCWCHAR ProgramName
	)
{
	const BOOLEAN Win8OrHigher = (RtlNtMajorVersion() >= 6 && RtlNtMinorVersion() >= 2) || RtlNtMajorVersion() > 6;
	const PCWCHAR CiOptionsName = Win8OrHigher ? L"g_CiOptions" : L"g_CiEnabled";
	Printf(L"\nUsage: %ls <COMMAND>\n\n"
		L"Commands:\n"
		L"    -a, --auto%18lsAuto: disable DSE, PG, HVCI (headless, no output)\n"
		L"    -p, --postboot%14lsAuto disable DSE + write registry markers (DCU)\n"
		L"    -c, --check%17lsTest EFI SetVariable hook\n"
		L"    -r, --read%18lsRead current %ls value\n"
		L"    -d, --disable%15lsDisable DSE\n"
		L"    -e, --enable%ls%2ls(Re)enable DSE\n"
		L"    -i, --info%18lsDump system info\n",
		ProgramName, L"", L"",
		CiOptionsName, L"",
		(Win8OrHigher ? L" [g_CiOptions]" : L"              "),
		L"", L"");
}

int wmain(int argc, wchar_t** argv)
{
	NT_ASSERT(argc != 0);

	ULONG CiOptionsValue = 0;
	NTSTATUS Status;
	BOOLEAN SeSystemEnvironmentWasEnabled = FALSE, SeDebugWasEnabled = FALSE;

	// EfiDSEFix structured exit codes
	const int EXIT_SUCCESS_CODE       = 0;
	const int EXIT_DSE_FAILED         = 0xDF000001;
	const int EXIT_DRIVER_NOT_LOADED  = 0xDF000002;
	const int EXIT_REGISTRY_FAILED    = 0xDF000003;
    
	int FinalExitCode = EXIT_SUCCESS_CODE;

	Status = RtlAdjustPrivilege(SE_SYSTEM_ENVIRONMENT_PRIVILEGE, TRUE, FALSE, &SeSystemEnvironmentWasEnabled);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}
	Status = RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, TRUE, FALSE, &SeDebugWasEnabled);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	if (argc <= 1 || argc > 3 ||
		(argc == 3 && wcstoul(argv[2], nullptr, 16) == 0) ||
		wcsncmp(argv[1], L"-h", sizeof(L"-h") / sizeof(WCHAR) - 1) == 0)
	{
		if (argc == 1)
		{
			// Show usage by default when run without arguments.
			// Headless mode is now triggered via the -a/--auto flag.
			PrintUsage(argv[0]);
			return 0;
		}
		// Print help text
		PrintUsage(argv[0]);
		return 0;
	}

	// Parse command line params
	const BOOLEAN Win8OrHigher = (RtlNtMajorVersion() >= 6 && RtlNtMinorVersion() >= 2) || RtlNtMajorVersion() > 6;
	const ULONG EnabledCiOptionsValue = Win8OrHigher ? 0x6 : CODEINTEGRITY_OPTION_ENABLED;
	const PCWCHAR CiOptionsName = Win8OrHigher ? L"g_CiOptions" : L"g_CiEnabled";
	BOOLEAN ReadOnly = FALSE;

	if (wcsncmp(argv[1], L"-r", sizeof(L"-r") / sizeof(WCHAR) - 1) == 0 ||
		wcsncmp(argv[1], L"--read", sizeof(L"--read") / sizeof(WCHAR) - 1) == 0)
	{
		CiOptionsValue = 0;
		ReadOnly = TRUE;
		Printf(L"Querying %ls value...\n", CiOptionsName);
	}
	else if (wcsncmp(argv[1], L"-d", sizeof(L"-d") / sizeof(WCHAR) - 1) == 0 ||
		wcsncmp(argv[1], L"--disable", sizeof(L"--disable") / sizeof(WCHAR) - 1) == 0)
	{
		CiOptionsValue = 0;
		Printf(L"Disabling DSE...\n");
	}
	else if (wcsncmp(argv[1], L"-e", sizeof(L"-e") / sizeof(WCHAR) - 1) == 0 ||
		wcsncmp(argv[1], L"--enable", sizeof(L"--enable") / sizeof(WCHAR) - 1) == 0)
	{
		if (Win8OrHigher)
		{
			CiOptionsValue = argc == 3 ? wcstoul(argv[2], nullptr, 16) : EnabledCiOptionsValue;
			Printf(L"(Re)enabling DSE [%ls value = 0x%lX]...\n", CiOptionsName, CiOptionsValue);
		}
		else
		{
			CiOptionsValue = EnabledCiOptionsValue;
			Printf(L"(Re)enabling DSE...\n");
		}
	}
	else if (wcsncmp(argv[1], L"-c", sizeof(L"-c") / sizeof(WCHAR) - 1) == 0 ||
		wcsncmp(argv[1], L"--check", sizeof(L"--check") / sizeof(WCHAR) - 1) == 0)
	{
		Printf(L"Checking for working EFI SetVariable hook...\n");
		Status = TestSetVariableHook();
		if (NT_SUCCESS(Status)) // Any errors have already been printed
			Printf(L"Success.\n");
		goto Exit;
	}
	else if (wcsncmp(argv[1], L"-i", sizeof(L"-i") / sizeof(WCHAR) - 1) == 0 ||
		wcsncmp(argv[1], L"--info", sizeof(L"--info") / sizeof(WCHAR) - 1) == 0)
	{
		Status = DumpSystemInformation();
		goto Exit;
	}
	else if (wcsncmp(argv[1], L"-a", sizeof(L"-a") / sizeof(WCHAR) - 1) == 0 ||
		wcsncmp(argv[1], L"--auto", sizeof(L"--auto") / sizeof(WCHAR) - 1) == 0)
	{
		// Headless auto mode: disable DSE silently, then exit
		CiOptionsValue = 0;
		Status = AdjustCiOptions(CiOptionsValue, nullptr, FALSE);
		goto Exit;
	}
	else if (wcsncmp(argv[1], L"-p", sizeof(L"-p") / sizeof(WCHAR) - 1) == 0 ||
		wcsncmp(argv[1], L"--postboot", sizeof(L"--postboot") / sizeof(WCHAR) - 1) == 0)
	{
		// DCU post-boot mode
		CiOptionsValue = 0;
		Status = AdjustCiOptions(CiOptionsValue, nullptr, FALSE);
		if (!NT_SUCCESS(Status))
		{
			FinalExitCode = (Status == STATUS_NOT_SUPPORTED || Status == STATUS_NO_SUCH_DEVICE) 
				? EXIT_DRIVER_NOT_LOADED : EXIT_DSE_FAILED;
			goto Exit;
		}

		// Write registry keys on success
		UNICODE_STRING KeyName;
		RtlInitUnicodeString(&KeyName, L"\\Registry\\Machine\\SOFTWARE\\Scootware\\DriverCompatibility");

		OBJECT_ATTRIBUTES ObjAttr;
		InitializeObjectAttributes(&ObjAttr, &KeyName, OBJ_CASE_INSENSITIVE, NULL, NULL);

		HANDLE hKey;
		NTSTATUS RegStatus = NtCreateKey(&hKey, KEY_ALL_ACCESS, &ObjAttr, 0, NULL, REG_OPTION_NON_VOLATILE, NULL);
		if (NT_SUCCESS(RegStatus))
		{
			LARGE_INTEGER SystemTime;
			NtQuerySystemTime(&SystemTime);

			UNICODE_STRING ValName;
			RtlInitUnicodeString(&ValName, L"PostBootLaunchTimestamp");
			NtSetValueKey(hKey, &ValName, 0, REG_QWORD, &SystemTime.QuadPart, sizeof(SystemTime.QuadPart));

			// Bypass method (assume 2 = SetVarHook for now if success via AdjustCiOptions)
			ULONG BypassMethod = 2; 
			RtlInitUnicodeString(&ValName, L"EfiMethod"); 
			NtSetValueKey(hKey, &ValName, 0, REG_DWORD, &BypassMethod, sizeof(BypassMethod));

			ULONG Installed = 1;
			RtlInitUnicodeString(&ValName, L"Installed");
			NtSetValueKey(hKey, &ValName, 0, REG_DWORD, &Installed, sizeof(Installed));

			NtClose(hKey);

			// Redundancy: Update marker file if present in the same directory
			// (ScootwareLoader places both EfiDSEFix.exe and ready.sig in C:\ProgramData\Scootware\DCU)
			WCHAR MarkerPath[MAX_PATH];
			const PRTL_USER_PROCESS_PARAMETERS Params = NtCurrentPeb()->ProcessParameters;
			UNICODE_STRING ImagePath = Params->ImagePathName;
			
			// Find last backslash
			LONG LastSlash = -1;
			for (USHORT i = 0; i < ImagePath.Length / sizeof(WCHAR); ++i) {
				if (ImagePath.Buffer[i] == L'\\') LastSlash = i;
			}

			if (LastSlash != -1 && LastSlash < (MAX_PATH - 12)) {
				memcpy(MarkerPath, ImagePath.Buffer, (LastSlash + 1) * sizeof(WCHAR));
				memcpy(MarkerPath + LastSlash + 1, L"ready.sig", 10 * sizeof(WCHAR));

				OBJECT_ATTRIBUTES MarkerAttr;
				UNICODE_STRING NtMarkerPath;
				WCHAR NtPathBuffer[MAX_PATH + 4];
				
				// Need to convert Win32 path to NT path if it's not already
				// For simplicity, we just use the existing buffer if it starts with the \??\ NT path prefix
				if (MarkerPath[0] == L'\\') {
					RtlInitUnicodeString(&NtMarkerPath, MarkerPath);
				} else {
					NtPathBuffer[0] = L'\\';
					NtPathBuffer[1] = L'?';
					NtPathBuffer[2] = L'?';
					NtPathBuffer[3] = L'\\';
					USHORT i = 0;
					while (MarkerPath[i] != L'\0' && i < MAX_PATH) {
						NtPathBuffer[4 + i] = MarkerPath[i];
						i++;
					}
					NtPathBuffer[4 + i] = L'\0';
					RtlInitUnicodeString(&NtMarkerPath, NtPathBuffer);
				}

				InitializeObjectAttributes(&MarkerAttr, &NtMarkerPath, OBJ_CASE_INSENSITIVE, NULL, NULL);
				IO_STATUS_BLOCK IoStatus;
				HANDLE hMarker;
				if (NT_SUCCESS(NtOpenFile(&hMarker, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &MarkerAttr, &IoStatus, 
					FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT))) 
				{
					// Marker file binary layout (64 bytes)
					// struct DcuMarker {
					//     uint32_t magic              = 0x55434421;   // "!DCU"
					//     uint32_t version            = 0;
					//     uint64_t installTimestamp   = 0;
					//     uint64_t postBootTimestamp  = 0; // offset 16
					//     ...
					// };
					LARGE_INTEGER Offset;
					Offset.QuadPart = 16;
					NtWriteFile(hMarker, NULL, NULL, NULL, &IoStatus, &SystemTime.QuadPart, sizeof(SystemTime.QuadPart), &Offset, NULL);
					NtClose(hMarker);
				}
			}
		}
		else
		{
			FinalExitCode = EXIT_REGISTRY_FAILED;
		}

		goto Exit;
	}
	else
	{
		PrintUsage(argv[0]);
		Status = STATUS_INVALID_PARAMETER;
		goto Exit;
	}

	// Call EFI runtime SetVariable service and write new value to g_CiOptions/g_CiEnabled
	ULONG OldCiOptionsValue;
	Status = AdjustCiOptions(CiOptionsValue, &OldCiOptionsValue, ReadOnly);

	// Print result
	if (!NT_SUCCESS(Status))
	{
		Printf(L"AdjustCiOptions failed: 0x%08lX\n", Status);
	}
	else
	{
		if (ReadOnly)
			Printf(L"Success.");
		else
			Printf(L"Successfully %ls DSE. Original", CiOptionsValue == 0 ? L"disabled" : L"(re)enabled");
		Printf(L" %ls value: 0x%lX\n", CiOptionsName, OldCiOptionsValue);
	}

Exit:
	// In auto mode or postboot mode, suppress all output and just return the status code
	if ((argc > 1 &&
		(wcsncmp(argv[1], L"-a", sizeof(L"-a") / sizeof(WCHAR) - 1) == 0 ||
		 wcsncmp(argv[1], L"--auto", sizeof(L"--auto") / sizeof(WCHAR) - 1) == 0 ||
		 wcsncmp(argv[1], L"-p", sizeof(L"-p") / sizeof(WCHAR) - 1) == 0 ||
		 wcsncmp(argv[1], L"--postboot", sizeof(L"--postboot") / sizeof(WCHAR) - 1) == 0)))
	{
		RtlAdjustPrivilege(SE_SYSTEM_ENVIRONMENT_PRIVILEGE, SeSystemEnvironmentWasEnabled, FALSE, &SeSystemEnvironmentWasEnabled);
		RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, SeDebugWasEnabled, FALSE, &SeDebugWasEnabled);
		return FinalExitCode != 0 ? FinalExitCode : Status;
	}

	RtlAdjustPrivilege(SE_SYSTEM_ENVIRONMENT_PRIVILEGE, SeSystemEnvironmentWasEnabled, FALSE, &SeSystemEnvironmentWasEnabled);
	RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, SeDebugWasEnabled, FALSE, &SeDebugWasEnabled);

	return FinalExitCode != 0 ? FinalExitCode : Status;
}

DECLSPEC_NOINLINE
static
VOID
ParseCommandLine(
	_In_ PWCHAR CommandLine,
	_Out_opt_ PWCHAR* Argv,
	_Out_opt_ PWCHAR Arguments,
	_Out_ PULONG Argc,
	_Out_ PULONG NumChars
	)
{
	*NumChars = 0;
	*Argc = 1;

	// Copy the executable name and and count bytes
	PWCHAR p = CommandLine;
	if (Argv != nullptr)
		*Argv++ = Arguments;

	// Handle quoted executable names
	BOOLEAN InQuotes = FALSE;
	WCHAR c;
	do
	{
		if (*p == '"')
		{
			InQuotes = !InQuotes;
			c = *p++;
			continue;
		}

		++*NumChars;
		if (Arguments != nullptr)
			*Arguments++ = *p;
		c = *p++;
	} while (c != '\0' && (InQuotes || (c != ' ' && c != '\t')));

	if (c == '\0')
		--p;
	else if (Arguments != nullptr)
		*(Arguments - 1) = L'\0';

	// Iterate over the arguments
	InQuotes = FALSE;
	for (; ; ++*NumChars)
	{
		if (*p != '\0')
		{
			while (*p == ' ' || *p == '\t')
				++p;
		}
		if (*p == '\0')
			break; // End of arguments

		if (Argv != nullptr)
			*Argv++ = Arguments;
		++*Argc;

		// Scan one argument
		for (; ; ++p)
		{
			BOOLEAN CopyChar = TRUE;
			ULONG NumSlashes = 0;

			while (*p == '\\')
			{
				// Count the number of slashes
				++p;
				++NumSlashes;
			}

			if (*p == '"')
			{
				// If 2N backslashes before: start/end a quote. Otherwise copy literally
				if ((NumSlashes & 1) == 0)
				{
					if (InQuotes && p[1] == '"')
						++p; // Double quote inside a quoted string
					else
					{
						// Skip first quote and copy second
						CopyChar = FALSE; // Don't copy quote
						InQuotes = !InQuotes;
					}
				}
				NumSlashes >>= 1;
			}

			// Copy slashes
			while (NumSlashes--)
			{
				if (Arguments != nullptr)
					*Arguments++ = '\\';
				++*NumChars;
			}

			// If we're at the end of the argument, go to the next
			if (*p == '\0' || (!InQuotes && (*p == ' ' || *p == '\t')))
				break;

			// Copy character into argument
			if (CopyChar)
			{
				if (Arguments != nullptr)
					*Arguments++ = *p;
				++*NumChars;
			}
		}

		if (Arguments != nullptr)
			*Arguments++ = L'\0';
	}
}

NTSTATUS
NTAPI
NtProcessStartupW(
	_In_ PPEB Peb
	)
{
	// On Windows XP (heh...) rcx does not contain a PEB pointer, but garbage
	Peb = Peb != nullptr ? NtCurrentPeb() : NtCurrentTeb()->ProcessEnvironmentBlock; // And this turd is to get Resharper to shut up about assigning to Peb before reading from it. Note LHS == RHS

	// Get the command line from the startup parameters. If there isn't one, use the executable name
	const PRTL_USER_PROCESS_PARAMETERS Params = RtlNormalizeProcessParams(Peb->ProcessParameters);
	const PWCHAR CommandLineBuffer = Params->CommandLine.Buffer == nullptr || Params->CommandLine.Buffer[0] == L'\0'
		? Params->ImagePathName.Buffer
		: Params->CommandLine.Buffer;

	// Count the number of arguments and characters excluding quotes
	ULONG Argc, NumChars;
	ParseCommandLine(CommandLineBuffer,
					nullptr,
					nullptr,
					&Argc,
					&NumChars);

	// Allocate a buffer for the arguments and a pointer array
	const ULONG ArgumentArraySize = (Argc + 1) * sizeof(PVOID);
	PWCHAR *Argv = static_cast<PWCHAR*>(
		RtlAllocateHeap(RtlProcessHeap(),
						HEAP_ZERO_MEMORY,
						ArgumentArraySize + NumChars * sizeof(WCHAR)));
	if (Argv == nullptr)
		return NtTerminateProcess(NtCurrentProcess, STATUS_NO_MEMORY);

	// Copy the command line arguments
	ParseCommandLine(CommandLineBuffer,
					Argv,
					reinterpret_cast<PWCHAR>(&Argv[Argc + 1]),
					&Argc,
					&NumChars);

	// Call the main function and terminate with the exit status
	const NTSTATUS Status = wmain(static_cast<int>(Argc), Argv);
	return NtTerminateProcess(NtCurrentProcess, Status);
}
