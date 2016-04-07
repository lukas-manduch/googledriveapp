#include <curl/curl.h>
#include <iostream>
#include "curl.h"

Curl::Curl()
{
	curl = curl_easy_init();
}

Curl::~Curl()
{
	curl_easy_cleanup(curl);
	if (list != nullptr)
		curl_slist_free_all(list);
}

Curl & Curl::set_type(Curl::Request_type type)
{
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, get_method(type).c_str());
	return *this;
}

Curl & Curl::set_url(const std::string & url)
{
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	return *this;
}

Curl & Curl::use_ssl(bool ssl)
{
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, ssl);
	return *this;
}

Curl & Curl::add_header(const std::string & header)
{
	list = curl_slist_append(list, header.c_str());
	return *this;
}

unsigned int Curl::send(const std::string& message, std::vector<char>& response)
{
	if (list != nullptr)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, message.size());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Curl::WriteVectorCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

	if (curl_easy_perform(curl) != 0)
		throw std::exception{ "Error while performing request" };
	int http_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	return http_code;
}

size_t Curl::WriteVectorCallback(void * ptr, size_t size, size_t count, std::vector<char>* mem)
{
	try
	{
		mem->reserve(size*count);
		mem->insert(mem->end(), static_cast<char*>(ptr), static_cast<char*>(ptr) + (size * count));
	}
	catch (...)
	{
		return 0; // Generate error
	}
	
	return size*count;
}

const std::string Curl::get_method(Request_type type)
{
	switch (type)
	{
		case Request_type::post:
			return std::string { "POST" };
			break;
		case Request_type::put:
			return std::string { "PUT" };
			break;
		case Request_type::get:
			return std::string { "GET" };
			break;
	}

    throw std::runtime_error("Invalid request type.");
}
