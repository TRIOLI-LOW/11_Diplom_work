#include "http_utils.h"

#include <regex>
#include <iostream>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <openssl/ssl.h>
#include <regex>
#include <boost/locale.hpp>
#include <unordered_map>
#include <sstream>
#include <pqxx/pqxx>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ip = boost::asio::ip;
namespace ssl = boost::asio::ssl;

using tcp = boost::asio::ip::tcp;

bool isText(const boost::beast::multi_buffer::const_buffers_type& b)
{
	for (auto itr = b.begin(); itr != b.end(); itr++)
	{
		for (int i = 0; i < (*itr).size(); i++)
		{
			if (*((const char*)(*itr).data() + i) == 0)
			{
				return false;
			}
		}
	}

	return true;
}


// Функция удаления тегов и очистки теста
std::string cleanHtml(const std::string& html) {
	// Удаляем HTML-теги
	std::regex tags("<[^>]+>");
	std::string text = std::regex_replace(html, tags, " ");

	// Убираем знаки препинания
	std::regex symbols("[^a-zA-Zа-яА-Я0-9 ]+");
	text = std::regex_replace(text, symbols, " ");

	// Приводим к нижнему регистру
	text = boost::locale::to_lower(text);

	return text;
}
// Разбиваем текст на слова и считаем частоту
std::unordered_map<std::string, int> countWordFrequency(const std::string& text) {
	std::unordered_map<std::string, int> wordCount;
	std::istringstream iss(text);
	std::string word;

	while (iss >> word) {
		if (word.length() >= 3 && word.length() <= 32) {  // Отбрасываем лишние слова
			wordCount[word]++;
		}
	}

	return wordCount;
}


void saveToDatabase(const std::string& url, const std::unordered_map<std::string, int>& wordFrequency) {
	try {
		pqxx::connection conn("dbname=seach_db user=postgres password=911215171 host=localhost port=5432");
		pqxx::work txn(conn);

		// Сохраняем URL
		txn.exec("INSERT INTO documents (url) VALUES (" + txn.quote(url) + ") ON CONFLICT (url) DO NOTHING");

		// Получаем ID страницы
		pqxx::result r = txn.exec("SELECT id FROM documents WHERE url = " + txn.quote(url));
		std::cout << "Row content: " << r[0][0].c_str() << std::endl; 

		if (r.empty()) {
			std::cout << "No rows returned for URL: " << url << std::endl;
			return;
		}
		int doc_id = r[0]["id"].as<int>();

		// Сохраняем слова и их частоту
		for (const auto& [word, frequency] : wordFrequency) {
			txn.exec("INSERT INTO words (word) VALUES (" + txn.quote(word) + ") ON CONFLICT (word) DO NOTHING");

			// Получаем ID слова
			pqxx::result word_r = txn.exec("SELECT id FROM words WHERE word = " + txn.quote(word));
			int word_id = word_r[0]["id"].as<int>();

			// Записываем частоту слова
			txn.exec("INSERT INTO word_frequencies (doc_id, word_id, frequency) VALUES (" +
				std::to_string(doc_id) + ", " + std::to_string(word_id) + ", " + std::to_string(frequency) +
				") ON CONFLICT (doc_id, word_id) DO UPDATE SET frequency = word_frequencies.frequency + " +
				std::to_string(frequency));
		}

		txn.commit();
	}
	catch (const std::exception& e) {
		std::cerr << "Database error: " << e.what() << std::endl;
	}
}

std::string getHtmlContent(const Link& link)
{

	std::string result;

	try
	{
		std::string host = link.hostName;
		std::string query = link.query;

		net::io_context ioc;

		if (link.protocol == ProtocolType::HTTPS)
		{

			ssl::context ctx(ssl::context::tlsv13_client);
			ctx.set_default_verify_paths();

			beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
			stream.set_verify_mode(ssl::verify_none);

			stream.set_verify_callback([](bool preverified, ssl::verify_context& ctx) {
				return true; // Accept any certificate
				});


			if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
				beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
				throw beast::system_error{ec};
			}

			ip::tcp::resolver resolver(ioc);
			get_lowest_layer(stream).connect(resolver.resolve({ host, "https" }));
			get_lowest_layer(stream).expires_after(std::chrono::seconds(30));


			http::request<http::empty_body> req{http::verb::get, query, 11};
			req.set(http::field::host, host);
			req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

			stream.handshake(ssl::stream_base::client);
			http::write(stream, req);

			beast::flat_buffer buffer;
			http::response<http::dynamic_body> res;
			http::read(stream, buffer, res);

			if (isText(res.body().data()))
			{
				result = buffers_to_string(res.body().data());
			}
			else
			{
				std::cout << "This is not a text link, bailing out..." << std::endl;
			}

			beast::error_code ec;
			stream.shutdown(ec);
			if (ec == net::error::eof) {
				ec = {};
			}

			if (ec) {
				throw beast::system_error{ec};
			}
		}
		else
		{
			tcp::resolver resolver(ioc);
			beast::tcp_stream stream(ioc);

			auto const results = resolver.resolve(host, "http");

			stream.connect(results);

			http::request<http::string_body> req{http::verb::get, query, 11};
			req.set(http::field::host, host);
			req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);


			http::write(stream, req);

			beast::flat_buffer buffer;

			http::response<http::dynamic_body> res;


			http::read(stream, buffer, res);

			if (isText(res.body().data()))
			{
				result = buffers_to_string(res.body().data());
			}
			else
			{
				std::cout << "This is not a text link, bailing out..." << std::endl;
			}

			beast::error_code ec;
			stream.socket().shutdown(tcp::socket::shutdown_both, ec);

			if (ec && ec != beast::errc::not_connected)
				throw beast::system_error{ec};

		}
	}
	catch (const std::exception& e)
	{
		std::cout << e.what() << std::endl;
	}

	return result;
}

