#pragma once
#include <string>
class Cloud_uploader
{
public:
	enum class File_type
	{
		text,
	};
	_Check_return_ virtual bool upload_file(_In_ const std::wstring& path ) = 0;
	_Check_return_ virtual bool upload_file(_In_ const std::wstring& path , File_type type) = 0;

	_Check_return_ virtual bool upload_folder(_In_ const std::wstring& path) = 0;

	virtual ~Cloud_uploader() {};
};
