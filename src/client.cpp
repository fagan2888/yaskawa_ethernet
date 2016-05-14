#include "client.hpp"
#include "encode.hpp"
#include "decode.hpp"

#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/connect.hpp>

#include <memory>
#include <atomic>

namespace dr {
namespace yaskawa {

namespace {
	using Resolver = boost::asio::ip::tcp::resolver;
	using Socket   = boost::asio::ip::tcp::socket;

	/// Struct holding details for a connection attempt.
	struct ConnectionAttempt {
		EthernetClient::Callback callback;
		std::atomic<bool> finished{false};
		Socket * socket;
		Resolver resolver;
		boost::asio::steady_timer timer;

		explicit ConnectionAttempt(boost::asio::ip::tcp::socket & socket, EthernetClient::Callback callback) :
			callback(std::move(callback)),
			socket(&socket),
			resolver(socket.get_io_service()),
			timer(socket.get_io_service()) {}
	};

	/// Called when a connection attempt finished.
	void onConnect(boost::system::error_code const & error, Resolver::iterator iterator, std::shared_ptr<ConnectionAttempt> attempt) {
		(void) iterator;

		if (attempt->finished.exchange(true)) {
			boost::system::error_code discard_error;
			attempt->socket->close(discard_error);
			return;
		}

		attempt->finished = true;
		attempt->timer.cancel();
		attempt->callback(error);
	}

	/// Called when the resolver has a result.
	void onResolve(boost::system::error_code const & error, Resolver::iterator iterator, std::shared_ptr<ConnectionAttempt> attempt) {
		if (error) {
			if (attempt->finished.exchange(true)) return;
			attempt->callback(error);
		}

		if (attempt->finished.load()) return;

		auto connect_callback = std::bind(onConnect, std::placeholders::_1, std::placeholders::_2, attempt);
		boost::asio::async_connect(*attempt->socket, iterator, connect_callback);
	}

	/// Called when a connection attempt times out.
	void onConnectTimeout(boost::system::error_code const & error, std::shared_ptr<ConnectionAttempt> attempt) {
		if (attempt->finished.exchange(true)) return;
		attempt->resolver.cancel();
		attempt->socket->cancel();

		if (error) return attempt->callback(error);
		attempt->callback(boost::asio::error::timed_out);
	}
}

EthernetClient::EthernetClient(boost::asio::io_service & ios) : socket_(ios) {};

void EthernetClient::connect(std::string const & host, std::string const & port, Callback const & callback) {
	connect(host, port, 0, callback);
}

void EthernetClient::connect(std::string const & host, std::string const & port, unsigned int timeout, Callback const & callback) {
	auto attempt          = std::make_shared<ConnectionAttempt>(socket_, callback);
	auto resolve_callback = std::bind(&onResolve, std::placeholders::_1, std::placeholders::_2, attempt);
	attempt->resolver.async_resolve({host, port}, resolve_callback);

	if (timeout) {
		attempt->timer.expires_from_now(std::chrono::milliseconds(timeout));
		attempt->timer.async_wait(std::bind(onConnectTimeout, std::placeholders::_1, attempt));
	}
}

void EthernetClient::start(int keep_alive, ResultCallback<std::string> const & callback) {
	std::ostream stream(&write_buffer_);
	encodeStartRequest(stream, keep_alive);

	auto write_handler = [this, callback] (boost::system::error_code const & error, std::size_t bytes_transferred) {
		if (error) return callback(error);
		write_buffer_.consume(bytes_transferred);

		auto read_handler = [this, callback] (boost::system::error_code const & error, std::size_t bytes_transferred) {
			if (error) return callback(error);
			string_view data = {boost::asio::buffer_cast<char const *>(read_buffer_.data()), bytes_transferred};
			callback(decodeResponse(data));
		};
		boost::asio::async_read_until(socket_, read_buffer_, ResponseMatcher{}, read_handler);
	};

	boost::asio::async_write(socket_, write_buffer_.data(), write_handler);
}

}}
