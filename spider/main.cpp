#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <unordered_map>
#include "http_utils.h"
#include <functional>
#include "../ini_parser.h"


std::mutex mtx;
std::condition_variable cv;
std::queue<std::function<void()>> tasks;
bool exitThreadPool = false;


void threadPoolWorker() {
	std::unique_lock<std::mutex> lock(mtx);
	while (!exitThreadPool || !tasks.empty()) {
		if (tasks.empty()) {
			cv.wait(lock);
		}
		else {
			auto task = tasks.front();
			tasks.pop();
			lock.unlock();
			task();
			lock.lock();
		}
	}
}
void parseLink(const Link& link, int depth)
{
	try {

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		std::string html = getHtmlContent(link);
		if (html.size() == 0)
		{
			std::cout << "Failed to get HTML Content" << std::endl;
			return;
		}


		std::string text = cleanHtml(html);
		std::cout << "Extracted text: "<< std::endl;
		std::cout << text << std::endl;

		// Подсчитываем частоту слов
		std::unordered_map<std::string, int> wordFrequency = countWordFrequency(text);

		// Сохранение данных в базу	
		
		std::string fullUrl = "https://" + link.hostName + link.query;

		std::cout << "**********************Saving to database: " << fullUrl << std::endl;


		saveToDatabase(fullUrl, wordFrequency);


		 std::vector<Link> links = extractLinks(html, link.hostName, link.protocol);

		 if (depth > 0) {
			 std::lock_guard<std::mutex> lock(mtx);
			 for (auto& subLink : links) {
				 tasks.push([subLink, depth]() { parseLink(subLink, depth - 1); });
			 }
			cv.notify_one();
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

}



int main()
{
	initializeDatabase();

	IniParser ini("../../../config.ini");
	std::string start_url = ini.getValue<std::string>("Spider", "start_url");
	int depth = ini.getValue<int>("Spider", "depth");

	// Разбираем URL на протокол, хост и путь
	std::string protocol = start_url.substr(0, start_url.find("://"));
	std::string url_without_protocol = start_url.substr(start_url.find("://") + 3);
	std::string host = url_without_protocol.substr(0, url_without_protocol.find('/'));
	std::string query = url_without_protocol.substr(url_without_protocol.find('/'));

	ProtocolType proto = (protocol == "https") ? ProtocolType::HTTPS : ProtocolType::HTTP;
	Link link{ proto, host, query };
	try {
		int numThreads = std::thread::hardware_concurrency();
		std::vector<std::thread> threadPool;

		for (int i = 0; i < numThreads; ++i) {
			threadPool.emplace_back(threadPoolWorker);
		}



		{
			std::lock_guard<std::mutex> lock(mtx);
			tasks.push([link, depth]() { parseLink(link,depth); });
			cv.notify_one();
		}


		std::this_thread::sleep_for(std::chrono::seconds(2));


		{
			std::lock_guard<std::mutex> lock(mtx);
			exitThreadPool = true;
			cv.notify_all();
		}

		for (auto& t : threadPool) {
			t.join();
		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}
	return 0;
}
