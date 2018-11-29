// WriteZipToSetup.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace std;

BOOL CALLBACK EnumResLangProc(HMODULE hModule, LPCTSTR lpszType, LPCTSTR lpszName, WORD wIDLanguage, LONG_PTR lParam)
{
	HANDLE hUpdate = (HANDLE)lParam;
	HRSRC hFindItAgain = FindResourceEx(hModule, lpszType, lpszName, wIDLanguage);

	HGLOBAL hGlobal = LoadResource(hModule, hFindItAgain);
	if (!hGlobal) return true;

	UpdateResource(hUpdate, lpszType, lpszName, wIDLanguage, LockResource(hGlobal), SizeofResource(hModule, hFindItAgain));
	return true;
}

BOOL CALLBACK EnumResNameProc(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
	HANDLE hUpdate = (HANDLE)lParam;

	EnumResourceLanguages(hModule, lpszType, lpszName, EnumResLangProc, (LONG_PTR)hUpdate);
	return true;
}

BOOL CALLBACK EnumResTypeProc(HMODULE hMod, LPTSTR lpszType, LONG_PTR lParam) 
{
	std::vector<wchar_t*>* typeList = (std::vector<wchar_t*>*)lParam;
	if (IS_INTRESOURCE(lpszType)) {
		typeList->push_back(lpszType);
	} else {
		typeList->push_back(_wcsdup(lpszType));
	}

	return true;
}

int CopyResourcesToStubExecutable(wchar_t* src, wchar_t* dest) 
{
	HMODULE hSrc = LoadLibraryEx(src, NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (!hSrc) return GetLastError();

	HANDLE hUpdate = BeginUpdateResource(dest, true);
	if (!hUpdate) return GetLastError();

	std::vector<wchar_t*> typeList;
	EnumResourceTypes(hSrc, EnumResTypeProc, (LONG_PTR)&typeList);

	for (auto& type : typeList) {
		EnumResourceNames(hSrc, type, EnumResNameProc, (LONG_PTR)hUpdate);
	}

	EndUpdateResource(hUpdate, false);
	return 0;
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc > 1 && wcscmp(argv[1], L"--copy-stub-resources") == 0) {
		if (argc != 4) goto fail;
		return CopyResourcesToStubExecutable(argv[2], argv[3]);
	}

	// First three arguments are required (EXE, target setup EXE and ZIP-path)
	int argsIndex = 3;
	if (argc < argsIndex) {
		goto fail;
	}

	wchar_t* pwszSetRequiredFramework = NULL;
	wchar_t* pwszRequireUac = NULL;
	while (argc > argsIndex)
	{
		if (wcscmp(argv[argsIndex], L"--set-required-framework") == 0) {
			if (argc <= ++argsIndex) {
				goto fail;
			}
			pwszSetRequiredFramework = argv[argsIndex++];
		} else if (wcscmp(argv[argsIndex], L"--require-uac") == 0) {
			if (argc <= ++argsIndex) {
				goto fail;
			}
			pwszRequireUac = argv[argsIndex++];
		}
	}

	wprintf(L"Setup: %s, Zip: %s\n", argv[1], argv[2]);

	// Read the entire zip file into memory, yolo
	HANDLE hFile = CreateFile(argv[2], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		printf("Can't open Zip file\n");
		return -1;
	}

	BY_HANDLE_FILE_INFORMATION fileInfo;
	if (!GetFileInformationByHandle(hFile, &fileInfo)) {
		goto fail;
	}

	BYTE* pBuf = new BYTE[fileInfo.nFileSizeLow + 0x1000];
	BYTE* pCurrent = pBuf;
	DWORD dwBytesRead;

	printf("Starting to read in Zip file!\n");
	do {
		if (!ReadFile(hFile, pCurrent, 0x1000, &dwBytesRead, NULL)) {
			printf("Failed to read file! 0x%p\n", GetLastError());
			goto fail;
		}

		pCurrent += dwBytesRead;
	} while (dwBytesRead > 0);

	printf("Updating Resource!\n");
	HANDLE hRes = BeginUpdateResource(argv[1], false);
	if (!hRes) {
		printf("Couldn't open setup.exe for writing\n");
		goto fail;
	}

	if (!UpdateResource(hRes, L"DATA", (LPCWSTR)131, 0x0409, pBuf, fileInfo.nFileSizeLow)) {
		printf("Failed to update resource\n");
		goto fail;
	}

	if (pwszSetRequiredFramework) {
		if (!UpdateResource(hRes, L"FLAGS", (LPCWSTR)132, 0x0409, pwszSetRequiredFramework, (wcslen(pwszSetRequiredFramework)+1) * sizeof(wchar_t))) {
			printf("Failed to update resouce\n");
			goto fail;
		}
	}

	if (pwszRequireUac) {
		if (!UpdateResource(hRes, L"FLAGS", (LPCWSTR)133, 0x0409, pwszRequireUac, (wcslen(pwszRequireUac) + 1) * sizeof(wchar_t))) {
			printf("Failed to update resouce\n");
			goto fail;
		}
	}

	printf("Finished!\n");
	if (!EndUpdateResource(hRes, false)) {
		printf("Failed to update resource\n");
		goto fail;
	}

	printf("It worked!\n");
	return 0;

fail:
	printf("Usage: WriteZipToSetup [Setup.exe template] [Zip File]\n");
	return -1;
}