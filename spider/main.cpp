#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "http_utils.h"
#include <functional>


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

		// TODO: Parse HTML code here on your own

		std::cout << "html content:" << std::endl;
		std::cout << html << std::endl;

		// TODO: Collect more links from HTML code and add them to the parser like that:

		std::vector<Link> links = {
			{ProtocolType::HTTPS, "en.wikipedia.org", "/wiki/Wikipedia"},
			{ProtocolType::HTTPS, "wikimediafoundation.org", "/"},
		};

		if (depth > 0) {
			std::lock_guard<std::mutex> lock(mtx);

			size_t count = links.size();
			size_t index = 0;
			for (auto& subLink : links)
			{
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
	try {
		int numThreads = std::thread::hardware_concurrency();
		std::vector<std::thread> threadPool;

		for (int i = 0; i < numThreads; ++i) {
			threadPool.emplace_back(threadPoolWorker);
		}

		Link link{ ProtocolType::HTTPS, "en.wikipedia.org", "/wiki/Main_Page" };


		{
			std::lock_guard<std::mutex> lock(mtx);
			tasks.push([link]() { parseLink(link, 1); });
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
