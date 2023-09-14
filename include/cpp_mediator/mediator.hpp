#ifndef HOLDEN_MEDIATOR_HPP_
#define HOLDEN_MEDIATOR_HPP_

#include <stdexcept>
#include <vector>
#include <memory>
#include <future>

#include "async.hpp"

#include "Hypodermic/Container.h"

namespace holden {
    
    // An optional pure-virtual struct to mark request types
    template <typename TResponse>
    struct request {
        using response_type = TResponse;
        virtual ~request() = default;
    };


    struct request_handler_base {
        virtual ~request_handler_base() = default;
        virtual std::shared_ptr<void> _handle(void* request, cancellationToken& cancellation) { return nullptr; }
    };

    template <typename TRequest>
    struct request_handler : request_handler_base {
        virtual ~request_handler() = default;
        virtual std::shared_ptr<void> _handle(void* request, cancellationToken& cancellation) override {
            return static_cast<std::shared_ptr<void>>(handle(*static_cast<TRequest*>(request), cancellation));
        }
        virtual std::shared_ptr<typename TRequest::response_type> handle(TRequest& request, cancellationToken& cancellation) = 0;
    };
    


    class mediator {
    protected:
        std::shared_ptr<Hypodermic::Container> m_container;

        template<typename TRequest>
        inline std::shared_ptr<typename TRequest::response_type> handle(const std::shared_ptr<request_handler<TRequest>>& handler, TRequest& request, cancellationToken& cancellation)
        {
            return handler->handle(request, cancellation);
        }

    public:
        mediator(std::shared_ptr<Hypodermic::Container> a_container) : m_container(a_container) {}

        template<typename TRequest>
        auto send(TRequest& r, cancellationToken& cancellation) -> std::vector<std::shared_ptr<typename TRequest::response_type>>
        {

            auto ret = std::vector<std::shared_ptr < typename TRequest::response_type>>();

            auto handlers = m_container->resolveAll<request_handler<TRequest>>();
            for (auto handler : handlers)
            {
                ret.push_back(handle(handler, r, cancellation));
            }

            return ret;
        }

        template<typename TRequest>
        auto sendAsync(TRequest& r, cancellationToken& cancellation) -> futures<typename TRequest::response_type>
        {
            auto self = this;

            auto ret = std::vector<std::future<taskResult<typename TRequest::response_type>>>();

            auto handlers = m_container->resolveAll<request_handler<TRequest>>();
            for (auto handler : handlers)
            {
                std::future<taskResult<typename TRequest::response_type>> ftr = std::async(std::launch::async, [handler, &self, &r, &cancellation]() {
                    taskResult<typename TRequest::response_type> taskResult;
                    
                    try
                    {
                        taskResult.result = self->handle(handler, r, cancellation);
                    }
                    catch (timeoutException& e)
                    {
                        taskResult.exception = std::make_shared<timeoutException>(e);
                    }
                    catch (cancellationException& e)
                    {
                        taskResult.exception = std::make_shared<cancellationException>(e);
                    }
                    catch (std::exception& e)
                    {
                        taskResult.exception = std::make_shared<std::exception>(e);
                    }
                    
                    return taskResult;
                });
                ret.push_back(std::move(ftr));
            }

            return futures<typename TRequest::response_type>(ret);
        }



        virtual ~mediator() {}
    };

} // namespace holden

#endif // HOLDEN_MEDIATOR_HPP_
