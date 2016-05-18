#include "tcp/client.hpp"
#include "tcp/protocol.hpp"
#include "../impl/connect.hpp"

#include <memory>
#include <atomic>
#include <utility>

namespace dr {
namespace yaskawa {
namespace tcp {

Client::Client(boost::asio::io_service & ios) : socket_(ios) {};

void Client::connect(std::string const & host, std::string const & port, unsigned int timeout, Callback const & callback) {
	asyncResolveConnect({host, port}, timeout, socket_, callback);
}

void Client::connect(std::string const & host, std::uint16_t port, unsigned int timeout, Callback const & callback) {
	connect(host, std::to_string(port), timeout, callback);
}

void Client::start(int keep_alive, ResultCallback<CommandResponse> const & callback) {
	//encodeStartRequest(std::ostream(&write_buffer_), keep_alive);
	//sendStartRequest(socket_, read_buffer_, write_buffer_, callback);
}

}}}