//
// Created by elizavetamaikova on 05.04.2021.
//

#ifndef SUGGESTION_SERVER_HPP
#define SUGGESTION_SERVER_HPP

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <nlohmann/json.hpp>
#include <utility>
#include <vector>
#include <string>
#include <ostream>
#include <strstream>
#include <fstream>
#include <shared_mutex>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

template<class Stream>
struct send_lambda
{
  Stream& stream_;
  bool& close_;
  beast::error_code& ec_;

  explicit
  send_lambda(
      Stream& stream,
      bool& close,
      beast::error_code& ec)
      : stream_(stream)
      , close_(close)
      , ec_(ec)
  {
  }

//  template<bool isRequest, class Body, class Fields>
  void operator()(http::response<http::string_body>&& res) const
  {
    // Determine if we should close the connection after
//    close_ = msg.need_eof();
//      close_ = res.keep_alive();

    // We need the serializer here because the serializer requires
    // a non-const file_body, and the message oriented version of
    // http::write only works with const messages.
//    http::serializer<isRequest, Body, Fields> sr{msg};
    http::write(stream_, res, ec_);
  }
};

struct JstoVec
{
  std::string id;
  std::string name;
  uint32_t cost;
};

class Server {
 public:
  Server();
  void fail(beast::error_code ec, char const* what);
  void do_session(tcp::socket& socket);
//  template< class Body, class Allocator, class Send>
  void handle_request(
      http::request<http::string_body>&& req,
      send_lambda<tcp::socket>&& send);

  void update_collections();
  void sort_collection();
  std::string create_responce(std::string id);

 private:
  nlohmann::json _js;
  std::vector<JstoVec> _vec;
  std::shared_mutex _lock;

};

#endif  // SUGGESTION_SERVER_HPP
