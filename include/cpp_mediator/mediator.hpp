#ifndef HOLDEN_MEDIATOR_HPP_
#define HOLDEN_MEDIATOR_HPP_

#include <stdexcept>
#include <vector>
#include <memory>
#include <future>

#include "async.hpp"

#include "Hypodermic/Container.h"

namespace holden {
    
    template <typename TResponse>
    struct request {
        using response_type = TResponse;
        virtual ~request() = default;
    };

    template <typename TRequest>
    struct request_handler {
        virtual ~request_handler() = default;
        virtual std::shared_ptr<typename TRequest::response_type> handle(TRequest& request, cancellationToken& cancellation) = 0;
    };

    template <typename TRequest>
    struct request_middleware {
        std::shared_ptr<request_middleware<TRequest>> next;
        virtual ~request_middleware() = default;
        virtual std::shared_ptr<typename TRequest::response_type> handle(TRequest& request, cancellationToken& cancellation) = 0;
    };
    


    class mediator {
    protected:
        std::shared_ptr<Hypodermic::Container> m_container;

        template <typename TRequest>
        struct request_middleware_head : public request_middleware<TRequest> {
            const std::shared_ptr<request_handler<TRequest>> head_handler;
            inline request_middleware_head(const std::shared_ptr<request_handler<TRequest>>& a_head_handler) : head_handler(a_head_handler) {}
            std::shared_ptr<typename TRequest::response_type> handle(TRequest& request, cancellationToken& cancellation) override
            {
                return head_handler->handle(request, cancellation);
            }
        };
        
        template<typename TRequest>
        static inline std::shared_ptr<typename TRequest::response_type> handle(std::shared_ptr<Hypodermic::Container> container, const std::shared_ptr<request_handler<TRequest>>& handler, TRequest& request, cancellationToken& cancellation)
        {
            do
            {
                if (container == nullptr)
                    break;

                auto middleware = container->resolveAll<request_middleware<TRequest>>();

                if (middleware.size() == 0)
                    break;

                std::shared_ptr<request_middleware<TRequest>> prev_middleware =
                    std::shared_ptr<request_middleware<TRequest>>(new request_middleware_head<TRequest>(handler));


                for (int i = middleware.size() - 1; i >= 0; i--)
                {
                    auto curr_middleware = middleware[i];
                    curr_middleware->next = prev_middleware;
                    prev_middleware = curr_middleware;
                }

                return middleware[0]->handle(request, cancellation);
            } while (false);

            return handler->handle(request, cancellation);
        }

    public:
        mediator(std::shared_ptr<Hypodermic::Container> a_container) : m_container(a_container) {}

        template<typename TRequest>
        inline auto send(TRequest& r, cancellationToken& cancellation) -> std::vector<std::shared_ptr<typename TRequest::response_type>>
        {

            auto ret = std::vector<std::shared_ptr < typename TRequest::response_type>>();

            auto handlers = m_container->resolveAll<request_handler<TRequest>>();
            for (auto handler : handlers)
            {
                ret.push_back(handle(m_container, handler, r, cancellation));
            }

            return ret;
        }

        template<typename TRequest>
        inline auto sendAsync(TRequest& r, cancellationToken& cancellation) -> futures<typename TRequest::response_type>
        {
            auto ret = std::vector<std::future<taskResult<typename TRequest::response_type>>>();
            auto container = m_container;
            auto handlers = m_container->resolveAll<request_handler<TRequest>>();
            for (auto handler : handlers)
            {
                std::future<taskResult<typename TRequest::response_type>> ftr = std::async(std::launch::async, [handler, container, &r, &cancellation]() {
                    taskResult<typename TRequest::response_type> taskResult;
                    
                    try
                    {
                        taskResult.result = handle(container, handler, r, cancellation);
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

            return futures<typename TRequest::response_type>(ret, cancellation);
        }



        virtual ~mediator() {}
    };

} // namespace holden

#endif // HOLDEN_MEDIATOR_HPP_
