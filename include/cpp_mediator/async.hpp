#pragma once

#include <future>
#include <vector>
#include <memory>
#include <chrono>


struct cancellationException : public std::exception {
    const char* what() const override {
        return "Task was canceled";
    }
};

class cancellationToken {
    
    std::shared_ptr<bool> is_cancelled = std::make_shared<bool>(false);

protected:
    
    inline virtual void throwIt() {
        throw cancellationException();
    }
    
public:
    inline cancellationToken() { }
    
    inline cancellationToken(const cancellationToken& other) {
        is_cancelled = other.is_cancelled;
    }
    
    inline cancellationToken(cancellationToken&& other) noexcept {
        is_cancelled = other.is_cancelled;
    }
    
    inline void cancel() {
        *is_cancelled = true;
    }

    inline virtual bool isCanceled() {
        return *is_cancelled;
    }
    
    inline void throwIfCanceled() {
        if (isCanceled()) {
            throwIt();
        }
    }
};

struct timeoutException : public cancellationException {
    const char* what() const override {
        return "Task timed out";
    }
};

class timeoutToken : public cancellationToken {
    std::chrono::milliseconds m_timeout;
    std::chrono::time_point<std::chrono::steady_clock> m_start;
    
protected:
    
    inline void throwIt() override {
        throw timeoutException();
    }
    
public:
    
    inline timeoutToken(std::chrono::milliseconds timeout) : m_timeout(timeout), m_start(std::chrono::steady_clock::now()) { }
    
    inline timeoutToken(int timeout) : timeoutToken(std::chrono::milliseconds(timeout)) { }

    inline bool isCanceled() override {
        return cancellationToken::isCanceled() || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start) > m_timeout;
    }
};

template<typename TResult>
struct taskResult {
    std::shared_ptr<TResult> result;
    std::shared_ptr<std::exception> exception;

    inline taskResult() : result(nullptr), exception(nullptr) { }
    
    inline taskResult(std::shared_ptr<TResult> result) : result(result), exception(nullptr) { }
    
    inline taskResult(std::shared_ptr<std::exception> exception) : result(nullptr), exception(exception) { }
    
    inline bool hasResult() {
        return result != nullptr;
    }
    
    inline bool hasException() {
        return exception != nullptr;
    }

    inline TResult getResult() {
        if (hasException()) {
            std::rethrow_exception(exception);
        }
        return *result;
    }
    
    inline bool withResult(std::function<void(std::shared_ptr<TResult>)> action) {
        if (hasResult()) {
            action(result);
            return true;
        }
        return false;
    }

    inline bool withException(std::function<void(std::shared_ptr<std::exception>)> action) {
        if (hasException()) {
            action(exception);
            return true;
        }
        return false;
    }
    
    inline bool getResult(std::shared_ptr<TResult>& res) {
        if (hasResult()) {
            res = result;
            return true;
        }
        return false;
    }

    inline bool getException(std::shared_ptr<std::exception>& ex) {
        if (hasException()) {
            ex = exception;
            return true;
        }
        return false;
    }

    inline bool isCanceled() {
        return hasException() && dynamic_cast<cancellationException*>(exception.get()) != nullptr;
    }

    inline bool isTimeout() {
        return hasException() && dynamic_cast<timeoutException*>(exception.get()) != nullptr;
    }
};

template<typename TResult>
class taskResults : public std::vector<taskResult<TResult>> {
    
public:
    
    inline taskResults(std::vector<taskResult<TResult>> results) : std::vector<taskResult<TResult>>(std::move(results)) { }
    
    inline taskResults() { }

    inline void throwFirstException()
    {
        for (auto& res : *this) {
            if (res.hasException()) {
                std::rethrow_exception(res.exception);
            }
        }
    }

    inline bool hasResult() {
        for (auto& res : *this) {
            if (res.hasResult()) {
                return true;
            }
        }
        return false;
    }

    inline bool hasException() {
        for (auto& res : *this) {
            if (res.hasException()) {
                return true;
            }
        }
        return false;
    }

    inline TResult getResult() {
        for (auto& res : *this) {
            if (res.hasResult()) {
                return res.getResult();
            }
        }
        throwFirstException();
    }

    inline void withResults(std::function<void(std::shared_ptr<TResult>)> action) {
        for (auto& res : *this) {
            res.withResult(action);
        }
    }

    inline void withExceptions(std::function<void(std::shared_ptr<std::exception>)> action) {
        for (auto& res : *this) {
            res.withException(action);
        }
    }

    inline bool withFirstResult(std::function<void(std::shared_ptr<TResult>)> action) {
        for (auto& res : *this) {
            if (res.withResult(action)) {
                return true;
            }
        }
        return false;
    }

    inline bool withFirstException(std::function<void(std::shared_ptr<std::exception>)> action) {
        for (auto& res : *this) {
            if (res.withException(action)) {
                return true;
            }
        }
        return false;
    }

    inline bool getFirstResult(std::shared_ptr<TResult>& res) {
        for (auto& r : *this) {
            if (r.getResult(res)) {
                return true;
            }
        }
        return false;
    }

    inline bool getFirstException(std::shared_ptr<std::exception>& ex) {
        for (auto& r : *this) {
            if (r.getException(ex)) {
                return true;
            }
        }
        return false;
    }
};


template<typename TResult>
class futures {
    std::vector<std::future<taskResult<TResult>>> m_futures;
    cancellationToken m_token;
    
public:
    inline futures(std::vector<std::future<taskResult<TResult>>>& futures, cancellationToken& a_token)
        : m_futures(std::move(futures))
        , m_token(a_token)
    {}

    inline bool isReady()
    {
        for (auto& ftr : m_futures) {
            if (ftr.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                return true;
            }
        }
        return false;
    }

    inline bool wait()
    {
        while (true)
        {
            if (m_token.isCanceled()) {
                return false;
            }
            
            for (auto& ftr : m_futures) {
                if (ftr.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                    continue;
                }
            }
            
            return true;
        }
    }
    
    inline taskResults<TResult> get() {
        std::vector<taskResult<TResult>> ret;
        for (auto& ftr : m_futures) {
            ret.push_back(ftr.get());
        }
        return taskResults<TResult>(ret);
    }
};