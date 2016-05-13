#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/steady_timer.hpp>

#include <functional>

#include "messages.hpp"

namespace dr {
namespace yaskawa {

class EthernetClient {
public:
	using Socket   = boost::asio::ip::tcp::socket;
	using Callback = std::function<void (boost::system::error_code const & error)>;

	template<typename T>
	using ResultCallback = std::function<void (ErrorOr<T> const & result)>;

private:
	Socket socket_;
	boost::asio::streambuf read_buffer_;

public:
	EthernetClient(boost::asio::io_service & ios);

	/// Open a connection.
	void connect(
		std::string const & host,   ///< Hostname or IP address to connect to.
		std::string const & port,   ///< Port or service name to connect to.
		unsigned int timeout,       ///< Timeout for the connection attempt in milliseconds.
		Callback const & callback   ///< Callback to call when the connection attempt finished.
	);

	/// Open a connection.
	void connect(
		std::string const & host,   ///< Hostname or IP address to connect to.
		std::string const & port,   ///< Port or service name to connect to.
		Callback const & callback   ///< Callback to call when the connection attempt finished.
	);

	/// Close the connection.
	void close();

	boost::asio::io_service & ios() { return socket_.get_io_service(); }

	void start(int keep_alive, ResultCallback<Response> const & callback);

	void readByteVariable(ResultCallback<std::uint8_t> const & callback);
	void readIntegerVariable(ResultCallback<int> const & callback);
	void readDoubleVariable(ResultCallback<double> const & callback);
	void readStringVariable(ResultCallback<double> const & callback);

	void writeVariable(Callback const & callback);
};

}}
