#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "system/jsonhelper.hpp"

#include <Windows.h>

#include <format>
#include <limits>

int rawFileRead(const std::string& fileName, char** data, uint bufferSize)
{
	*data = nullptr;

	const HANDLE hFile = CreateFileA(
		fileName.c_str(),
		GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_READONLY,
		nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		TC_LOG_ERROR(std::format("rawFileRead: failed to open '{}' (error {})", fileName, GetLastError()).c_str());
		return -1;
	}

	LARGE_INTEGER fileSize = {};
	if (!GetFileSizeEx(hFile, &fileSize))
	{
		const DWORD sizeError = GetLastError();
		CloseHandle(hFile);
		TC_LOG_ERROR(std::format("rawFileRead: GetFileSizeEx failed for '{}' (error {})", fileName, sizeError).c_str());
		return -1;
	}

	if (fileSize.QuadPart < 0)
	{
		CloseHandle(hFile);
		TC_LOG_ERROR(std::format("rawFileRead: negative file size reported for '{}'", fileName).c_str());
		return -1;
	}

	const UINT64 readSizeWide = static_cast<UINT64>(fileSize.QuadPart);

	const UINT64 cap = (bufferSize == 0) ? static_cast<UINT64>(BUFFERSIZE) : static_cast<UINT64>(bufferSize);
	if (readSizeWide > static_cast<UINT64>(std::numeric_limits<DWORD>::max()) || readSizeWide > cap)
	{
		CloseHandle(hFile);
		TC_LOG_ERROR(std::format("rawFileRead: '{}' is {} bytes, larger than the {} byte read cap; refusing to truncate", fileName, readSizeWide, cap).c_str());
		return -1;
	}

	if (readSizeWide > static_cast<UINT64>(std::numeric_limits<int>::max()))
	{
		CloseHandle(hFile);
		TC_LOG_ERROR(std::format("rawFileRead: '{}' is {} bytes, larger than INT_MAX; cannot be returned as int", fileName, readSizeWide).c_str());
		return -1;
	}

	const uint readSize = static_cast<uint>(readSizeWide);

	char* buffer = new char[static_cast<size_t>(readSize) + 1];

	DWORD bytesRead = 0;
	const BOOL success = ReadFile(hFile, buffer, static_cast<DWORD>(readSize), &bytesRead, nullptr);
	const DWORD readError = success ? ERROR_SUCCESS : GetLastError();
	CloseHandle(hFile);

	if (!success)
	{
		delete[] buffer;
		TC_LOG_ERROR(std::format("rawFileRead: ReadFile failed for '{}' (error {})", fileName, readError).c_str());
		return -1;
	}

	if (bytesRead != readSize)
	{
		delete[] buffer;
		TC_LOG_ERROR(std::format("rawFileRead: short read for '{}' ({} of {} bytes)", fileName, bytesRead, readSize).c_str());
		return -1;
	}

	buffer[readSize] = '\0';
	*data = buffer;
	return static_cast<int>(readSize);
}
