#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include <string>
#include <iostream>
#include "../ini_parser.h"
#include "http_connection.h"
#include <Windows.h>


void httpServer(tcp::acceptor& acceptor, tcp::socket& socket)
{
	acceptor.async_accept(socket,
		[&](beast::error_code ec)
		{
			if (!ec)
				std::make_shared<HttpConnection>(std::move(socket))->start();
			httpServer(acceptor, socket);
		});
}


int main(int argc, char* argv[])
{
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
	boost::locale::generator gen;
	std::locale loc = gen("en_US.UTF-8"); // Устанавливаем локаль UTF-8
	std::locale::global(loc);


	try
	{

		auto const address = net::ip::make_address("0.0.0.0");

		IniParser ini(config_way);

		unsigned short port = ini.getValue<int>("Server", "port");;

		net::io_context ioc{1};

		tcp::acceptor acceptor{ioc, { address, port }};
		tcp::socket socket{ioc};
		httpServer(acceptor, socket);

		std::cout << "Open browser and connect to your port (see in config.ini) to see the web server operating" << std::endl;

		ioc.run();
	}
	catch (std::exception const& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}