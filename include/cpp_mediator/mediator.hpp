#ifndef HOLDEN_MEDIATOR_HPP_
#define HOLDEN_MEDIATOR_HPP_

#include <stdexcept>
#include <vector>
#include <memory>

namespace holden {

    // An optional pure-virtual struct to mark request types
    template <typename TResponse>
    struct request {
        using response_type = TResponse;
        virtual ~request() = default;
    };


    struct request_handler_base {
        virtual ~request_handler_base() = default;
        virtual std::shared_ptr<void> _handle(void* request) { return nullptr; }
    };

    template <typename TRequest>
    struct request_handler : request_handler_base {
        virtual ~request_handler() = default;
        virtual std::shared_ptr<void> _handle(void* request) override {
            return static_cast<std::shared_ptr<void>>(handle(*static_cast<TRequest*>(request)));
        }
        virtual std::shared_ptr<typename TRequest::response_type> handle(TRequest& request) = 0;
    };
    


    class mediator {
    protected:
        std::vector<std::shared_ptr<request_handler_base>> handlers_;

    public:
        mediator() {}

        template<typename THandler>
        void add_handler(std::shared_ptr<THandler>& handler) {
            std::shared_ptr<request_handler_base> hb = std::dynamic_pointer_cast<request_handler_base>(handler);
            handlers_.push_back(hb);
        }

        template<typename TRequest>
        auto send(TRequest& r) -> std::shared_ptr < typename TRequest::response_type> {
            for (std::shared_ptr<request_handler_base>& handler : handlers_) {
                std::shared_ptr<request_handler<TRequest>> typed_handler = std::dynamic_pointer_cast<request_handler<TRequest>>(handler);
                if (typed_handler) {
                    return typed_handler->handle(r);
                }
            }
            throw std::runtime_error("No handler found for request type");
        }

        virtual ~mediator() {}
    };

    template <typename... Args>
    mediator make_mediator(Args&... args) {
        return mediator(args...);
    }

} // namespace holden

#endif // HOLDEN_MEDIATOR_HPP_