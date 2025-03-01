#include "http_connection.h"
#include "../ini_parser.h"
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>
#include <iostream>
#include <vector>
#include <pqxx/pqxx>
#include <boost/beast/core.hpp> 
#include <boost/beast/http.hpp>


namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Функция для объединения слов с разделителем
std::string join(const std::vector<std::string>& vec, const std::string& delimiter) {
	std::ostringstream oss;
	for (size_t i = 0; i < vec.size(); ++i) {
		if (i != 0) oss << delimiter;  
		oss << vec[i];
	}
	return oss.str();  
}

// Функция для выполнения SQL-запроса
std::vector<std::string> executeSearchQuery(const std::string& query) {
	std::vector<std::string> resultUrls;
	try {
		// Подключение к базе данных
		IniParser ini("../../../config.ini"); 
		std::string db_host = ini.getValue<std::string>("Database", "host");
		std::string db_port = ini.getValue<std::string>("Database", "port");
		std::string db_name = ini.getValue<std::string>("Database", "dbname");
		std::string db_user = ini.getValue<std::string>("Database", "user");
		std::string db_password = ini.getValue<std::string>("Database", "password");

		// Формируем строку подключения
		std::string conn_str = "dbname=" + db_name + " user=" + db_user +
			" password=" + db_password + " host=" + db_host +
			" port=" + db_port;

		// Подключаемся к БД
		pqxx::connection conn(conn_str);
		pqxx::work txn(conn);

		// Выполнение SQL-запроса
		pqxx::result r = txn.exec(query);

		// Извлечение данных 
		for (auto row : r) {
			resultUrls.push_back(row["url"].c_str());
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Database error: " << e.what() << std::endl;
	}
	return resultUrls;
}


std::string url_decode(const std::string& encoded) {
	std::string res;
	std::istringstream iss(encoded);
	char ch;

	while (iss.get(ch)) {
		if (ch == '%') {
			int hex;
			iss >> std::hex >> hex;
			res += static_cast<char>(hex);
		}
		else {
			res += ch;
		}
	}

	return res;
}

std::string convert_to_utf8(const std::string& str) {
	std::string url_decoded = url_decode(str);
	return url_decoded;
}

HttpConnection::HttpConnection(tcp::socket socket)
	: socket_(std::move(socket))
{

}


void HttpConnection::start()
{
	readRequest();
	checkDeadline();
}


void HttpConnection::readRequest()
{
	auto self = shared_from_this();

	http::async_read(
		socket_,
		buffer_,
		request_,
		[self](beast::error_code ec,
			std::size_t bytes_transferred)
		{
			boost::ignore_unused(bytes_transferred);
			if (!ec)
				self->processRequest();
		});
}

void HttpConnection::processRequest()
{
	response_.version(request_.version());
	response_.keep_alive(false);

	switch (request_.method())
	{
	case http::verb::get:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponseGet();
		break;
	case http::verb::post:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponsePost();
		break;

	default:
		response_.result(http::status::bad_request);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body())
			<< "Invalid request-method '"
			<< std::string(request_.method_string())
			<< "'";
		break;
	}

	writeResponse();
}


void HttpConnection::createResponseGet()
{
	if (request_.target() == "/")
	{
		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Welcome!<p>\n"
			<< "<form action=\"/\" method=\"post\">\n"
			<< "    <label for=\"search\">Search:</label><br>\n"
			<< "    <input type=\"text\" id=\"search\" name=\"search\"><br>\n"
			<< "    <input type=\"submit\" value=\"Search\">\n"
			<< "</form>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else
	{
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::createResponsePost()
{

	if (request_.target() == "/")
	{
		std::string s = buffers_to_string(request_.body().data());

		std::cout << "POST data: " << s << std::endl;

		size_t pos = s.find('=');
		if (pos == std::string::npos)
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		std::string key = s.substr(0, pos);
		std::string value = s.substr(pos + 1);

		std::string utf8value = convert_to_utf8(value);

		if (key != "search")
		{
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		// TODO: Fetch your own search results here


		std::vector<std::string> search_terms;
		std::istringstream iss(utf8value);
		std::string word;
		while (std::getline(iss, word, '+')) {
			search_terms.push_back(word);
		}
	// Формируем SQL-запрос
		std::string query = "SELECT d.url, SUM(wf.frequency) AS relevance "
			"FROM documents d "
			"JOIN word_frequencies wf ON d.id = wf.doc_id "
			"JOIN words w ON wf.word_id = w.id "
			"WHERE w.word IN ('" + join(search_terms, "','") + "') "
			"GROUP BY d.url "
			"ORDER BY relevance DESC "
			"LIMIT 10;";

		// Здесь функция для выполнения SQL-запроса
		std::vector<std::string> searchResult = executeSearchQuery(query);

		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Response:<p>\n"
			<< "<ul>\n";

		for (const auto& url : searchResult) {

			beast::ostream(response_.body())
				<< "<li><a href=\""
				<< url << "\">"
				<< url << "</a></li>";
		}

		beast::ostream(response_.body())
			<< "</ul>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else
	{
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::writeResponse()
{
	auto self = shared_from_this();

	response_.content_length(response_.body().size());

	http::async_write(
		socket_,
		response_,
		[self](beast::error_code ec, std::size_t)
		{
			self->socket_.shutdown(tcp::socket::shutdown_send, ec);
			self->deadline_.cancel();
		});
}

void HttpConnection::checkDeadline()
{
	auto self = shared_from_this();

	deadline_.async_wait(
		[self](beast::error_code ec)
		{
			if (!ec)
			{
				self->socket_.close(ec);
			}
		});
}

