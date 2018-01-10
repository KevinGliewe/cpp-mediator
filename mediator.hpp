#ifndef _HOLDEN_MEDIATOR_H_
#define _HOLDEN_MEDIATOR_H_

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace holden {
namespace mediator {

class request_base {};

template <typename TResponse, typename THandler>
class request : public request_base {
 public:
  using response_type = TResponse;
  using handler_type = THandler;
  virtual ~request() {}
};

template <typename TRequest>
class request_handler {
 public:
  using request_type = TRequest;
  using response_type = typename TRequest::response_type;
  virtual typename TRequest::response_type handle(const TRequest& r) = 0;
  virtual ~request_handler() {}
};

class mediator {
 public:
  mediator() {}
  std::unordered_map<size_t, void*> handlers_by_type{};

  template <typename THandler>
  void register_handler(THandler* handler) {
    handlers_by_type.emplace(
      typeid(*handler).hash_code(), static_cast<void*>(handler)
    );
  }

  template<typename TRequest, typename = std::enable_if<std::is_base_of<request_base, TRequest>::value>>
  typename TRequest::response_type send(const TRequest& r) {
    auto hash = typeid(typename TRequest::handler_type).hash_code();
    for (decltype(handlers_by_type.size())
      i = 0; i < handlers_by_type.size(); ++i) {
      if (handlers_by_type.find(hash) != handlers_by_type.end()) {
        auto handler_ptr = static_cast<typename TRequest::handler_type*>(handlers_by_type[hash]);
        return handler_ptr->handle(r);
      }
    }

    return 1;
  }

  virtual ~mediator() {}
};

class req_handler;
class req : public request<int, req_handler> {};
class req_handler : public request_handler<req> {
 public:
  int handle(const req& r) {
    return 7;
  }
};

} // namespace mediator
} // namespace holden

#endif // _HOLDEN_MEDIATOR_H_

using namespace holden::mediator;

int main() {
  std::cout << "go\n";
  mediator m{};
  req r{};
  req_handler h{};

  std::cout << "registering...";
  m.register_handler(&h);
  std::cout << "registered\n";

  std::cout << "sending...";
  auto x = m.send(r);
  std::cout << "sent\n";

  std::cout << "final value: " << x << "\n\ndone.\n";
  return 0;
}
