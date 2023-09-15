// EventBusTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "mediator.hpp"
#include <iostream>
#include "Hypodermic/Hypodermic.h"

#include <thread>

struct NameProvider 
{
    std::string name = "John Doe";

    std::string get_name() { return name; }

    NameProvider(std::string name) : name(name) {}
};

// Define a request.
struct SayHello : holden::request<void> 
{
    // the output stream through which the handler should "say hello."
    std::ostream& output_stream;

    SayHello(std::ostream& out) : output_stream(out) {}
};

enum class FirstImpression { Good, Great, Stupendous, };

std::ostream& operator<<(std::ostream& os, FirstImpression i) {
    switch (i) {
    case FirstImpression::Good:       os << "good!";
    case FirstImpression::Great:      os << "great!!";
    case FirstImpression::Stupendous: os << "stupendous!!!";
    default: os << "off the charts!";
    }

    return os;
}

// Or we can skip the `request<>` interface and add `response_type` manually.
struct SayGoodbye 
{
    // Let mediator know what return type to infer.
    using response_type = FirstImpression;

    // the output stream through which the handler should "say hello."
    std::ostream& output_stream;

    SayGoodbye(std::ostream& out) : output_stream(out) {}
};

// Configure a request handler.
class SpeakerHello
    : public holden::request_handler<SayHello> 
{
public:
    // Say hello on the provided output stream.
    std::shared_ptr<void> handle(SayHello& r, cancellationToken& cancellation) override {
        r.output_stream << "Hello, world!\n";
        return nullptr;
    }

};

class SpeakerGoodbye
    :   public holden::request_handler<SayGoodbye>,
        public holden::request_handler<SayHello> 
{
public:
    
    std::shared_ptr<NameProvider> m_nameProvider;

    SpeakerGoodbye(std::shared_ptr<NameProvider> nameProvider) : m_nameProvider(nameProvider) {}

    // Say goodbye on the provided output stream, and return how the interaction 
    // went.
    std::shared_ptr<FirstImpression> handle(SayGoodbye& r, cancellationToken& cancellation) override {
        //while (true)
        //    cancellation.throwIfCanceled();
        r.output_stream << "Goodbye for now!\n";
        return std::make_shared<FirstImpression>(FirstImpression::Stupendous);
    }

    std::shared_ptr<void> handle(SayHello& r, cancellationToken& cancellation) override {
        
        r.output_stream << "Hello, " << m_nameProvider->get_name() << "!\n";
        return nullptr;
    }
};


struct FirstSayGoodlbyMiddleware : public holden::request_middleware<SayGoodbye>
{
    std::shared_ptr<FirstImpression> handle(SayGoodbye& r, cancellationToken& cancellation) override
    {
        std::cout << "FirstSayGoodlbyMiddleware::handle\n";
        return next->handle(r, cancellation);
    }
};

struct SecondSayGoodlbyMiddleware : public holden::request_middleware<SayGoodbye>
{
    std::shared_ptr<FirstImpression> handle(SayGoodbye& r, cancellationToken& cancellation) override
    {
        std::cout << "SecondSayGoodlbyMiddleware::handle\n";
        return next->handle(r, cancellation);
    }
};

struct SayHelloMiddleware : public holden::request_middleware<SayHello>
{
    std::shared_ptr<void> handle(SayHello& r, cancellationToken& cancellation) override
    {
        std::cout << "SayHelloMiddleware::handle\n";
        return next->handle(r, cancellation);
    }
};



int main() {
    
    Hypodermic::ContainerBuilder builder;
    builder.registerInstance(std::make_shared<NameProvider>("Cruel World"));
    builder.registerType<SpeakerHello>().as<holden::request_handler<SayHello>>();
    builder.registerType<SpeakerGoodbye>().as<holden::request_handler<SayHello>>();
    builder.registerType<SpeakerGoodbye>().as<holden::request_handler<SayGoodbye>>();
    builder.registerType<FirstSayGoodlbyMiddleware>().as<holden::request_middleware<SayGoodbye>>();
    builder.registerType<SecondSayGoodlbyMiddleware>().as<holden::request_middleware<SayGoodbye>>();
    builder.registerType<SayHelloMiddleware>().as<holden::request_middleware<SayHello>>();

    auto container = builder.build();
    
    holden::mediator mediator(container);

    auto cancelationToken = timeoutToken(1000);

    SayHello say_hello { std::cout };
    mediator.send(say_hello, cancelationToken);

    SayGoodbye say_goodbye { std::cout };
    auto first_impression = mediator.sendAsync(say_goodbye, cancelationToken).get();

    std::shared_ptr<FirstImpression> result;
    if (first_impression.getFirstResult(result))
    {
        std::cout << "The speaker's first impression was " << *result << "\n";
    }
    else if (first_impression[0].isTimeout())
    {
        std::cout << "The speaker timed out\n";
    }
    else if (first_impression[0].isCanceled())
    {
        std::cout << "The speaker was canceled\n";
    }
    else
    {
        std::cout << "The speaker threw an exception\n";
    }
    
    return 0;
}
