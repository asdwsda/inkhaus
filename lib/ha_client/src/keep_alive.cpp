#include "keep_alive.h"

std::string url_origin(const std::string& url) {
  size_t scheme = url.find("://");
  size_t from = scheme == std::string::npos ? 0 : scheme + 3;
  size_t slash = url.find('/', from);
  return slash == std::string::npos ? url : url.substr(0, slash);
}

HttpResponse KeepAliveTransport::request(const std::string& method, const std::string& url,
                                         const HttpHeaders& headers, const std::string& body) {
  const std::string origin = url_origin(url);
  if (!origin_.empty() && origin_ != origin) {
    raw_.drop();
    origin_.clear();
  }
  HttpResponse r = raw_.send(method, url, headers, body);
  if (r.status == 0 && r.stale) {  // the idle connection was closed by the server: once more, fresh
    raw_.drop();
    r = raw_.send(method, url, headers, body);
  }
  if (r.status == 0) {
    raw_.drop();
    origin_.clear();
  } else {
    origin_ = origin;
  }
  return r;
}

void KeepAliveTransport::close() {
  raw_.drop();
  origin_.clear();
}
