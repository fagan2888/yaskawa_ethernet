#pragma once
#include "tcp/protocol.hpp"
#include "error.hpp"
#include "string_view.hpp"

#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <system_error>
#include <utility>

namespace dr {
namespace yaskawa {
namespace tcp {

namespace impl {
	template<typename T> struct unwrap_error_or;
	template<typename T>
	struct unwrap_error_or<ErrorOr<T>> {
		using type = T;
	};

	template<typename T>
	using unwrap_error_or_t = typename unwrap_error_or<T>::type;

	template<typename Decoder>
	struct decoder_result {
		using type = unwrap_error_or_t<decltype(std::declval<Decoder>()(std::declval<string_view>()))>;
	};

	template<typename T>
	using decoder_result_t = typename decoder_result<T>::type;

	/// Class representing a command session.
	/**
	 * The session consists of the following actions:
	 * - Write command and data.
	 * - Read command response.
	 * - Read response data.
	 */
	template<typename Decoder, typename Callback, typename Socket, bool StartCommand>
	class CommandSession : public std::enable_shared_from_this<CommandSession<Decoder, Callback, Socket, StartCommand>> {
		using Ptr = std::shared_ptr<CommandSession>;
		using ResultType = decoder_result_t<Decoder>;

		Decoder decoder;
		Callback callback;
		Socket * socket;
		asio::streambuf * read_buffer;

	public:
		asio::streambuf command_buffer;
		asio::streambuf data_buffer;

		/// Construct a command session.
		CommandSession(Decoder decoder, Callback callback, Socket & socket, asio::streambuf & read_buffer) :
			decoder(std::move(decoder)),
			callback(std::move(callback)),
			socket(&socket),
			read_buffer(&read_buffer) {}

		/// Start the session by writing the command.
		void send() {
			auto callback = std::bind(&CommandSession::onWriteCommand, this, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2);
			asio::async_write(*socket, command_buffer.data(), callback);
		}

	protected:
		/// Called when the command has been written.
		void onWriteCommand(Ptr, std::error_code const & error, std::size_t bytes_transferred) {
			command_buffer.consume(bytes_transferred);
			if (error) callback(DetailedError(std::errc(error.value())));

			auto callback = std::bind(&CommandSession::onReadResponse<StartCommand>, this, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2);
			asio::async_read_until(*socket, *read_buffer, ResponseMatcher{}, callback);
		}

		/// Called when the command response has been read.
		/**
		 * If the we're a start session, only read the response and no data..
		 */
		template<bool R2 = StartCommand>
		typename std::enable_if<R2>::type
		onReadResponse(Ptr, std::error_code const & error, std::size_t bytes_transferred) {
			if (error) callback(DetailedError(std::errc(error.value())));
			ErrorOr<std::string> decoded = decodeCommandResponse(string_view{asio::buffer_cast<char const *>(read_buffer->data()), bytes_transferred});
			read_buffer->consume(bytes_transferred);
			return callback(decoded);
		}

		/// Called when the command response has been read.
		/**
		 * If the response type is not CommandResponse, read the data response too.
		 */
		template<bool R2 = StartCommand>
		typename std::enable_if<not R2>::type
		onReadResponse(Ptr, std::error_code const & error, std::size_t bytes_transferred) {
			if (error) callback(DetailedError(std::errc(error.value())));
			ErrorOr<std::string> decoded = decodeCommandResponse(string_view{asio::buffer_cast<char const *>(read_buffer->data()), bytes_transferred});
			read_buffer->consume(bytes_transferred);
			if (!decoded) return callback(decoded.error());

			// If the command has data, write it.
			if (data_buffer.size()) {
				auto callback = std::bind(&CommandSession::onWriteData, this, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2);
				asio::async_write(*socket, data_buffer.data(), callback);

			// Otherwise read the response directly.
			} else {
				auto callback = std::bind(&CommandSession::onReadData, this, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2);
				asio::async_read_until(*socket, *read_buffer, ResponseMatcher{}, callback);
			}
		}

		/// Called when the command data has been written.
		void onWriteData(Ptr, std::error_code const & error, std::size_t bytes_transferred) {
			if (error) callback(DetailedError(std::errc(error.value())));
			data_buffer.consume(bytes_transferred);

			auto callback = std::bind(&CommandSession::onReadData, this, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2);
			asio::async_read_until(*socket, *read_buffer, ResponseMatcher{}, callback);
		}

		/// Called when the command data has been read.
		void onReadData(Ptr, std::error_code const & error, std::size_t bytes_transferred) {
			if (error) callback(DetailedError(std::errc(error.value())));
			ErrorOr<ResultType> result = decoder(string_view{asio::buffer_cast<char const *>(read_buffer->data()), bytes_transferred});
			read_buffer->consume(bytes_transferred);
			callback(std::move(result));
		}
	};
}

template<typename Decoder, typename Callback, typename Socket>
auto makeCommandSesssion(Decoder && decoder, Callback && callback, Socket & socket, asio::streambuf & read_buffer) {
	using type = impl::CommandSession<std::decay_t<Decoder>, std::decay_t<Callback>, Socket, false>;
	return std::make_shared<type>(std::forward<Decoder>(decoder), std::forward<Callback>(callback), socket, read_buffer);
}

template<typename Callback, typename Socket>
auto makeStartCommandSesssion(Callback && callback, Socket & socket, asio::streambuf & read_buffer) {
	using type = impl::CommandSession<decltype(&decodeCommandResponse), std::decay_t<Callback>, Socket, true>;
	return std::make_shared<type>(decodeCommandResponse, std::forward<Callback>(callback), socket, read_buffer);
}

}}}
