#define CURL_STATICLIB
#define LIBXML2_STATICLIB
#define MQTT_STATICLIB

#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <chrono>
#include <cstring>

#include <mqtt/async_client.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

const std::string DFLT_SERVER_ADDRESS{ "localhost:8885" };
const std::string DFLT_CLIENT_ID{ "mqtt_id" };

const std::string KEY_STORE{ "ssl/client.key" };
const std::string TRUST_STORE{ "ssl/client.crt" };

const std::string LWT_TOPIC{ "events/disconnect" };
const std::string LWT_PAYLOAD{ "temperature sensor disconnected." };

const std::string S50_TOPIC{ "/api/temperature/S50" };
const std::string S107_TOPIC{ "/api/temperature/S107" };
const std::string S60_TOPIC{ "/api/temperature/S60" };

const std::string API_STATUS_TOPIC{ "/api/status" };

const int  QOS_mqtt = 1;
const auto TIMEOUT = std::chrono::seconds(10);

class callback : public virtual mqtt::callback
{
public:
	void connection_lost(const std::string& cause) override {
		std::cout << "\nConnection lost" << std::endl;
		if (!cause.empty())
			std::cout << "\tcause: " << cause << std::endl;
	}

	void delivery_complete(mqtt::delivery_token_ptr tok) override {
		std::cout << "\tDelivery complete for token: "
			<< (tok ? tok->get_message_id() : -1) << std::endl;
	}
};

struct Reading
{
	int ID;
	double value;
};

size_t curlWriteFunc(char* data, size_t size, size_t nmemb, std::string* buffer);
CURLcode performCurlRequest(const char* url, std::string& buffer, char errorBuffer[]);

std::vector<Reading>* parseReadings(std::string src, std::string& api_info);

int main(int argc, char* argv[])
{
	const char* url = "https://api.data.gov.sg/v1/environment/air-temperature";
	char curlErrorBuffer[CURL_ERROR_SIZE];
	std::string requestBuffer;

	if (performCurlRequest(url, requestBuffer, curlErrorBuffer) == CURLE_OK)
	{
		//Parse API

		std::string api_info;
		std::vector<Reading>* readings = parseReadings(requestBuffer, api_info);

		//MQTT connection
		std::string	address = DFLT_SERVER_ADDRESS,
			clientID = DFLT_CLIENT_ID;

		//Check sertificate files
		{
			std::ifstream tstore(TRUST_STORE);
			if (!tstore) {
				std::cerr << "The trust store file does not exist: " << TRUST_STORE << '\n';
				return 1;
			}

			std::ifstream kstore(KEY_STORE);
			if (!kstore) {
				std::cerr << "The key store file does not exist: " << KEY_STORE << '\n';
				return 1;
			}
		}

		std::cout << "Initializing for server '" << address << "'..." << '\n';
		mqtt::async_client client(address, clientID);

		//Set our callback class
		callback cb;
		client.set_callback(cb);

		// Configure SSL options

		auto sslopts = mqtt::ssl_options_builder()
			.trust_store(TRUST_STORE)
			.key_store(KEY_STORE)
			.enable_server_cert_auth(true)
			.error_handler([](const std::string& msg) {
			std::cerr << "SSL Error: " << msg << std::endl;
				})
			.finalize();

		// Configure last will message
		auto willmsg = mqtt::message(LWT_TOPIC, LWT_PAYLOAD, QOS_mqtt, true);

		// Configure connection options

		auto connopts = mqtt::connect_options_builder()
				.user_name("mickas")
				.password("alalot28")
				.will(std::move(willmsg))
				.ssl(std::move(sslopts))
				.connect_timeout(TIMEOUT)
				.finalize();

		std::cout << "  ...OK" << '\n';

		try {
			// Connect using SSL

			std::cout << "\nConnecting..." << '\n';
			mqtt::token_ptr conntok = client.connect(connopts);
			std::cout << "Waiting for the connection..." << '\n';
			conntok->wait();
			std::cout << "  ...OK" << '\n';

			//Create topics

			mqtt::topic s50_topic(client, S50_TOPIC, QOS_mqtt, true);
			mqtt::topic s107_topic(client, S107_TOPIC, QOS_mqtt, true);
			mqtt::topic s60_topic(client, S60_TOPIC, QOS_mqtt, true);
			mqtt::topic api_info_topic(client, API_STATUS_TOPIC, QOS_mqtt, true);

			// Publish readings
			
			for (auto it = readings->begin(); it != readings->end(); it++)
			{
				switch ((*it).ID)
				{
				case 50:
					client.publish(S50_TOPIC, std::to_string((*it).value));
					//s50_topic.publish(std::to_string((*it).value));
					break;
				case 107:
					client.publish(S107_TOPIC, std::to_string((*it).value));
					break;
				case 60:
					client.publish(S60_TOPIC, std::to_string((*it).value));
					break;
				default:
					break;
				}

			}
			//Publish api_status
			client.publish(API_STATUS_TOPIC, api_info);

			// Disconnect
			std::cout << "\nDisconnecting..." << '\n';
			client.disconnect()->wait();
			std::cout << "  ...OK" << '\n';
		}
		catch (const mqtt::exception& exc) {
			std::cerr << exc.what() << '\n';
			return 1;
		}
	}
	else
	{
		std::cout << curlErrorBuffer << '\n';
		return -1;
	}
	return 0;
}

CURLcode performCurlRequest(const char* url, std::string& buffer, char errorBuffer[])
{
	CURL* curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFunc);

		CURLcode curlResult = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		return curlResult;
	}
	return CURLE_UPLOAD_FAILED;
}

size_t curlWriteFunc(char* data, size_t size, size_t nmemb, std::string* buffer)
{
	size_t result = 0;

	if (buffer != NULL)
	{
		buffer->append(data, size * nmemb);
		result = size * nmemb;
	}
	return result;
}

std::vector<Reading>* parseReadings(std::string src, std::string& api_info)
{
	api_info = src.substr(
		src.find("status") + 9);
	api_info.erase(api_info.end() - 3, api_info.end());

	src = src.substr(
		src.find("readings") + 11);

	src.erase(
		src.begin() + src.find("]"),
		src.end());

	std::vector<Reading>* result = new std::vector<Reading>;

	while (src.find("}") != std::string::npos)
	{
		size_t stationDelim = src.find(':');
		size_t valueDelim = src.find(':', src.find(','));

		result->push_back(
			Reading{
				stoi(src.substr(stationDelim + 3, src.find(':', stationDelim + 3))),
				atof(src.substr(valueDelim + 1, src.find('}')).c_str()) });

		if (src.find('}') != src.rfind('}'))
			src.erase(src.begin(), src.begin() + src.find('}') + 2);
		else
			src.erase(src.begin(), src.end());
	}

	return result;
}
