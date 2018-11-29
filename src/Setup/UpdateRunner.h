#pragma once
class CUpdateRunner
{

public:
	static void DisplayErrorMessage(CString& errorMessage, wchar_t* logFile);
	static HRESULT IsUacRequired();
	static HRESULT AreWeUACElevated();
	static HRESULT ShellExecuteFromExplorer(LPWSTR pszFile, LPWSTR pszParameters, BOOL fRunElevated);
	static bool DirectoryExists(wchar_t* szPath);
	static bool DirectoryIsWritable(wchar_t* szPath);
	static int ExtractUpdaterAndRun(wchar_t* lpCommandLine, bool useFallbackDir);
};
