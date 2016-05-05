#pragma once

#include <vector>
#include <curl/curl.h>

class Curl final
{
//	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
public:
	enum class Request_type 
	{
		post,
		put,
		get
		// etc
	};
	
	// Constructors
	explicit Curl();

	Curl(const Curl&) = delete;
	Curl& operator=(const Curl&) = delete;
	
	~Curl();
	// -------------------------------

	Curl& set_type (Curl::Request_type);
	Curl& set_url ( _In_ const std::string& );
	Curl& use_ssl ( _In_ bool);
	Curl& add_header( _In_ const std::string&);

	//unsigned int send(const std::vector<char>& message, std::vector<char>& response );
	long send( _In_ const std::string& message, _Out_ std::vector<char>& response);
private:
	static size_t WriteVectorCallback( _In_ void *ptr, _In_  size_t size,_In_ size_t count,_Inout_ std::vector<char> *mem);
	const std::string get_method(Request_type) const;
	
    CURL* m_curl;
	struct curl_slist *m_list{ nullptr };
	bool m_used{ false };
};
