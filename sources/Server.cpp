//
// Created by elizavetamaikova on 05.04.2021.
//

#include "Server.hpp"

Server::Server()
{
  auto const address = net::ip::make_address("127.0.0.1");
  const uint port = 8080;

  std::thread{&Server::update_collections, this}.detach();
  net::io_context ioc{1};
  tcp::acceptor acceptor{ioc, {address, port}};
  for (;;)
  {
    tcp::socket socket{ioc};
    acceptor.accept(socket);
    std::thread{std::bind(
        &Server::do_session,
        this,
        std::move(socket))}.detach();
  }
}

void Server::fail(beast::error_code ec, char const* what)
{
  std::cerr << what << ": " << ec.message() << "\n";
}

void Server::do_session(tcp::socket& socket)
{
  bool close = false;
  beast::error_code ec;

  // This buffer is required to persist across reads
  beast::flat_buffer buffer;

  // This lambda is used to send messages
  send_lambda<tcp::socket> lambda{socket, close, ec};

  for (;;)
  {
    // Read a request
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if (ec == http::error::end_of_stream)
      break;
    if (ec)
      return fail(ec, "read");

    // Send the response
    handle_request(std::move(req), std::move(lambda));
    if (ec)
      return fail(ec, "write");
    if (close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      break;
    }
  }

  // Send a TCP shutdown
  socket.shutdown(tcp::socket::shutdown_send, ec);
}

//template< class Body, class Allocator, class Send>
void Server::handle_request(
    http::request<http::string_body>&& req,
    send_lambda<tcp::socket>&& send)
{
  // Returns a bad request response
  auto const bad_request =
      [&req](beast::string_view why)
      {
        http::response<http::string_body> res{http::status::bad_request,
                                              req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
      };

  // Returns a server error response
  auto const server_error =
      [&req](beast::string_view what)
      {
        http::response<http::string_body>
            res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
      };

  // Make sure we can handle the method
  if ( req.method() != http::verb::post &&
       req.target() != "v1/api/suggest")
    return send(bad_request("Unknown HTTP-method"));

  // Request path must be absolute and not contain "..".
  if ( req.target().empty() ||
       req.target()[0] != '/' ||
       req.target().find("..") != beast::string_view::npos)
    return send(bad_request("Illegal request-target"));

  beast::error_code ec;

  http::string_body::value_type body;
  // Handle an unknown error
  if (ec)
    return send(server_error(ec.message()));

  std::string id = req.body().substr(req.body().find(":") + 1);
  std::string sug = "";

  {
    _lock.lock_shared();
    sug = create_responce(id);
    _lock.unlock_shared();
  }

  body.append(sug);

  // Cache the size since we need it after the move
  auto const size = body.size();

  // Respond to Post request
  http::response<http::string_body> res{
      std::piecewise_construct,
      std::make_tuple(std::move(body)),
      std::make_tuple(http::status::ok, req.version())};

  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/plain");
  res.content_length(size);
  res.keep_alive(req.keep_alive());
  return send(std::move(res));
}


void Server::update_collections() {
  for (;;) {
    std::ifstream fl("../suggestions.json");
    std::stringstream ss;
    ss << fl.rdbuf();

    {
      _lock.lock();
      _js = nlohmann::json::parse(ss.str());
      sort_collection();
      _lock.unlock();
    }

    std::this_thread::sleep_for(std::chrono::minutes(15));
  }
}
void Server::sort_collection(){
  std::vector<JstoVec> sort_vec;
  for (auto js : _js)
    sort_vec.push_back(JstoVec{js["id"], js["name"], js["cost"]});

  for (uint32_t i = 0; i < sort_vec.size() - 1; ++i)
  {
    for (uint32_t j = 1; j < sort_vec.size(); ++j)
    {
      if (sort_vec[i].cost > sort_vec[j].cost)
        std::swap(sort_vec[i], sort_vec[j]);
    }
  }

  _vec = std::move(sort_vec);
}
std::string Server::create_responce(std::string id) {
  nlohmann::json res;
  uint32_t pos = 0;
  for (uint32_t i = 0; i < _vec.size(); ++i)
  {
    if (_vec[i].id == id)
    {
      auto js = nlohmann::json::object();
      js["text"] = _vec[i].name;
      js["position"] = pos;
      res["suggestion"].push_back(js);
      pos++;
    }
  }
  if (res.empty()) return "{\"suggestion\": []}";
  return res.dump();
}