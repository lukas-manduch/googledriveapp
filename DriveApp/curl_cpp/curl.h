// pimpl
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

	Curl(Curl&) = delete;
	Curl(Curl&&) = delete;

	Curl& operator=(Curl&) = delete;
	Curl& operator=(Curl&&) = delete;
	
	~Curl();
	// -------------------------------

	Curl& set_type (Curl::Request_type);
	Curl& set_url (const std::string& );
	Curl& use_ssl (bool);
	Curl& add_header(const std::string&);

	unsigned int send(const std::vector<char>& message, std::vector<char>& response );
	unsigned int send(const std::string& message, std::vector<char>& response);
private:
	static size_t WriteVectorCallback(void *ptr, size_t size, size_t count, std::vector<char> *mem);
	const std::string get_method(Request_type);
	CURL* curl;
	struct curl_slist *list{ nullptr };
};
