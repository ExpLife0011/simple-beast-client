#ifndef SIMPLE_HTTPCLIENT_HPP
#define SIMPLE_HTTPCLIENT_HPP

#include "client_private.hpp"
#include "client_private_http.hpp"
#include "url.hpp"
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <string>
#ifdef ENABLE_HTTPS
#include "client_private_ssl.hpp"
#endif

namespace simple_http {

/**
 * @brief The simple_client class a client for HTTP
 */

template <typename RequestBody, typename ResponseBody>
class basic_client : public std::enable_shared_from_this<basic_client<RequestBody, ResponseBody>> {
public:
  explicit basic_client(boost::asio::io_context &io, int timeoutMs = 3000)
      : m_io{io}, m_timeoutMs{timeoutMs}, m_state{Resolve} {
  }

  explicit basic_client(boost::asio::io_context &io,
                        std::function<void(boost::beast::http::request<RequestBody> &,
                                           boost::beast::http::response<ResponseBody> &)>
                            responseHandler,
                        int timeoutMs = 3000)
      : m_io{io}, m_responseHandler{responseHandler}, m_timeoutMs{timeoutMs} {
  }

  void get(const url &uri, int maxRedirects = 0, bool basicAuth = false, int version = 11) {
    performRequest(uri, boost::beast::http::verb::get, typename RequestBody::value_type{},
                   maxRedirects, basicAuth, version);
  }

  void post(const url &uri, typename RequestBody::value_type requestBody, int maxRedirects = 0,
            bool basicAuth = false, int version = 11) {
    performRequest(uri, boost::beast::http::verb::post, requestBody, maxRedirects, basicAuth,
                   version);
  }

  void performRequest(const url &uri, boost::beast::http::verb method, int maxRedirects = 0,
                      bool basicAuth = false, int version = 11) {
    performRequest(uri, method, typename RequestBody::value_type{}, maxRedirects, basicAuth,
                   version);
  }

  void performRequest(const url &uri, boost::beast::http::verb method,
                      typename RequestBody::value_type requestBody, int maxRedirects = 0,
                      bool basicAuth = false, int version = 11) {
    m_url = uri;
    if (m_url.hasAuthentication() && (m_username.empty() || m_password.empty())) {
      setAuthorization(m_url.username(), m_url.password(), basicAuth);
    }
    std::shared_ptr<client_private<RequestBody, ResponseBody>> httpclient =
        client_private<RequestBody, ResponseBody>::privateForRequest(m_url,
                                                                     this->shared_from_this());
    if (httpclient) {
      httpclient->performRequest(uri, method, requestBody, maxRedirects, basicAuth, version);
      m_p = httpclient;
    }
  }

  void setAuthorization(boost::string_view username, boost::string_view password,
                        bool forceBasic = false) {
    m_basicAuthForce = forceBasic;
    m_username = username.to_string();
    m_password = password.to_string();
  }

  inline void abort() {
    if (auto cl = m_p.lock()) {
      cl->abort();
    }
  }

  void setResponseHandler(std::function<void(boost::beast::http::request<RequestBody> &,
                                             boost::beast::http::response<ResponseBody> &)>
                              responseHandler) {
    m_responseHandler = responseHandler;
  }

  void setFailHandler(std::function<void(boost::beast::http::request<RequestBody> &,
                                         boost::beast::http::response<ResponseBody> &, fail_reason,
                                         boost::string_view)>
                          failHandler) {
    m_failHandler = failHandler;
  }

  client_state state() const {
    return m_state;
  }

private:
  boost::asio::io_context &m_io;
  url m_url;
  std::string m_username;
  std::string m_password;
  int m_maxRedirects;
  int m_timeoutMs;
  bool m_basicAuthForce;
  std::function<void(boost::beast::http::request<RequestBody> &,
                     boost::beast::http::response<ResponseBody> &)>
      m_responseHandler;
  std::function<void(boost::beast::http::request<RequestBody> &,
                     boost::beast::http::response<ResponseBody> &, fail_reason, boost::string_view)>
      m_failHandler;
  client_state m_state;

  boost::string_view userAgent() {
    return "simple-beast-client/1.0";
  }

  void failure(fail_reason reason, boost::string_view message) {
    static boost::beast::http::request<RequestBody> req;
    static boost::beast::http::response<ResponseBody> resp;
    try {
      m_failHandler(req, resp, reason, message);
    } catch (std::bad_function_call & /*f*/) {
      // Discard silently.
    }
  }

  friend class client_private<RequestBody, ResponseBody>;

  std::weak_ptr<client_private<RequestBody, ResponseBody>> m_p;
  friend class client_private_http<RequestBody, ResponseBody>;
#ifdef ENABLE_HTTPS
  friend class client_private_ssl<RequestBody, ResponseBody>;
#endif
};

typedef boost::beast::http::request<boost::beast::http::empty_body> empty_body_request;
typedef boost::beast::http::request<boost::beast::http::string_body> string_body_request;
typedef boost::beast::http::response<boost::beast::http::empty_body> empty_body_response;
typedef boost::beast::http::response<boost::beast::http::string_body> string_body_response;
typedef basic_client<boost::beast::http::empty_body, boost::beast::http::string_body> get_client;
typedef basic_client<boost::beast::http::string_body, boost::beast::http::string_body> post_client;

} // namespace simple_http

#endif // SIMPLE_HTTPCLIENT_HPP
