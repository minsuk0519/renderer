#include "system/jsonhelper.hpp"

int rawFileRead(std::string fileName, char** data, uint bufferSize)
{
	HANDLE hFile = CreateFileA(
		fileName.c_str(),
		GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_READONLY,
		nullptr);

	std::error_code errorCode;

	if (hFile == INVALID_HANDLE_VALUE)
	{
		errorCode = std::error_code(static_cast<int>(GetLastError()), std::system_category());
		return -1;
	}

	auto size = LARGE_INTEGER{};
	if (!GetFileSizeEx(hFile, &size))
	{
		errorCode = std::error_code(static_cast<int>(GetLastError()), std::system_category());
		CloseHandle(hFile);
		return -1;
	}

	uint fileBufferSize;
	if (bufferSize == 0)
	{
		fileBufferSize = BUFFERSIZE;
	}
	
	*data = new char[fileBufferSize];

	uint offset = 0;

	HANDLE m_handle = CreateEventA(nullptr, FALSE, FALSE, nullptr);

	auto error = std::error_code();
	if (m_handle == INVALID_HANDLE_VALUE)
	{
		error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
	}

	OVERLAPPED overlapped{};
	overlapped.hEvent = m_handle;
	overlapped.Offset = static_cast<DWORD>(offset);
	overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);

	DWORD bytesRead;

	bool success = ReadFile(hFile, reinterpret_cast<void**>(*data), static_cast<DWORD>(BUFFERSIZE), &bytesRead, nullptr);

	(*data)[bytesRead] = '\0';

	if (!success)
	{
		if (auto ec = GetLastError(); ec != ERROR_IO_PENDING)
		{
			error = std::error_code(ec, std::system_category());
		}

		return -1;
	}

	// It is always good practice to close the open file handles even though
	// the app will exit here and clean up open handles anyway.
	CloseHandle(hFile);

	return bytesRead;
}