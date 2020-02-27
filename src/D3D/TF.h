#pragma once

#include <Windows.h>
#include <wrl.h>
#include <comdef.h>
#include <cassert>
#include <string>

extern const int gNumFrameResources;

inline std::wstring AnsiToWString(const std::string& str) {
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
	return std::wstring(buffer);
}

class DxException
{
public:
	std::wstring FunctionName;
	std::wstring Filename;
	HRESULT ErrorCode = S_OK;
	int LineNumber = -1;

public:
	DxException() = default;
	DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
		ErrorCode(hr), FunctionName(functionName), Filename(filename), LineNumber(lineNumber) {};

	std::wstring ToString() const {
		// Get the string decription of the error code.
		_com_error err(ErrorCode);
		std::wstring msg = err.ErrorMessage();
		return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
	};
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x) {	\
	HRESULT hr__ = (x);		\
	if(FAILED(hr__)) {		\
		std::wstring wfn = AnsiToWString(__FILE__);	\
		throw DxException(hr__, L#x, wfn, __LINE__);\
	}	\
}
#define TF(x) ThrowIfFailed(x)
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if((x)) { (x)->Release(); (x) = nullptr; } }
#endif
