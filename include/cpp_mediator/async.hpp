#pragma once

#include <future>
#include <vector>
#include <memory>


struct cancellationException : public std::exception {
    const char* what() const override {
        return "Task was canceled";
    }
};

class cancellationToken {
    
    std::shared_ptr<bool> is_cancelled = std::make_shared<bool>(false);

protected:
    
    virtual void throwIt() {
        throw cancellationException();
    }
    
public:
    cancellationToken() { }
    
    cancellationToken(const cancellationToken& other) {
        is_cancelled = other.is_cancelled;
    }
    
    cancellationToken(cancellationToken&& other) noexcept {
        is_cancelled = other.is_cancelled;
    }
    
    void cancel() {
        *is_cancelled = true;
    }

    virtual bool isCanceled() {
        return *is_cancelled;
    }
    
    void throwIfCanceled() {
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
    
    void throwIt() override {
        throw timeoutException();
    }
    
public:
    
    timeoutToken(std::chrono::milliseconds timeout) : m_timeout(timeout), m_start(std::chrono::steady_clock::now()) { }
    
    timeoutToken(int timeout) : timeoutToken(std::chrono::milliseconds(timeout)) { }

    bool isCanceled() override {
        return cancellationToken::isCanceled() || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start) > m_timeout;
    }
};

template<typename TResult>
struct taskResult {
    std::shared_ptr<TResult> result;
    std::shared_ptr<std::exception> exception;

    taskResult() : result(nullptr), exception(nullptr) { }
    
    taskResult(std::shared_ptr<TResult> result) : result(result), exception(nullptr) { }
    
    taskResult(std::shared_ptr<std::exception> exception) : result(nullptr), exception(exception) { }
    
    bool hasResult() {
        return result != nullptr;
    }
    
    bool hasException() {
        return exception != nullptr;
    }

    TResult getResult() {
        if (hasException()) {
            std::rethrow_exception(exception);
        }
        return *result;
    }
    
    bool withResult(std::function<void(std::shared_ptr<TResult>)> action) {
        if (hasResult()) {
            action(result);
            return true;
        }
        return false;
    }

    bool withException(std::function<void(std::shared_ptr<std::exception>)> action) {
        if (hasException()) {
            action(exception);
            return true;
        }
        return false;
    }
    
    bool getResult(std::shared_ptr<TResult>& res) {
        if (hasResult()) {
            res = result;
            return true;
        }
        return false;
    }

    bool getException(std::shared_ptr<std::exception>& ex) {
        if (hasException()) {
            ex = exception;
            return true;
        }
        return false;
    }

    bool isCanceled() {
        return hasException() && dynamic_cast<cancellationException*>(exception.get()) != nullptr;
    }

    bool isTimeout() {
        return hasException() && dynamic_cast<timeoutException*>(exception.get()) != nullptr;
    }
};

template<typename TResult>
class taskResults : public std::vector<taskResult<TResult>> {
    
public:
    
    taskResults(std::vector<taskResult<TResult>> results) : std::vector<taskResult<TResult>>(std::move(results)) { }
    
    taskResults() { }

    void throwFirstException()
    {
        for (auto& res : *this) {
            if (res.hasException()) {
                std::rethrow_exception(res.exception);
            }
        }
    }

    bool hasResult() {
        for (auto& res : *this) {
            if (res.hasResult()) {
                return true;
            }
        }
        return false;
    }

    bool hasException() {
        for (auto& res : *this) {
            if (res.hasException()) {
                return true;
            }
        }
        return false;
    }

    TResult getResult() {
        for (auto& res : *this) {
            if (res.hasResult()) {
                return res.getResult();
            }
        }
        throwFirstException();
    }

    void withResults(std::function<void(std::shared_ptr<TResult>)> action) {
        for (auto& res : *this) {
            res.withResult(action);
        }
    }

    void withExceptions(std::function<void(std::shared_ptr<std::exception>)> action) {
        for (auto& res : *this) {
            res.withException(action);
        }
    }

    bool withFirstResult(std::function<void(std::shared_ptr<TResult>)> action) {
        for (auto& res : *this) {
            if (res.withResult(action)) {
                return true;
            }
        }
        return false;
    }

    bool withFirstException(std::function<void(std::shared_ptr<std::exception>)> action) {
        for (auto& res : *this) {
            if (res.withException(action)) {
                return true;
            }
        }
        return false;
    }

    bool getFirstResult(std::shared_ptr<TResult>& res) {
        for (auto& r : *this) {
            if (r.getResult(res)) {
                return true;
            }
        }
        return false;
    }

    bool getFirstException(std::shared_ptr<std::exception>& ex) {
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
    
public:
    futures(std::vector<std::future<taskResult<TResult>>>& futures) : m_futures(std::move(futures)) {}

    void wait() {
        for (auto& ftr : m_futures) {
            ftr.wait();
        }
    }
    
    taskResults<TResult> get() {
        std::vector<taskResult<TResult>> ret;
        for (auto& ftr : m_futures) {
            ret.push_back(ftr.get());
        }
        return taskResults<TResult>(ret);
    }
};