// The code below demonstrates simple usage of the mediator library.

#include "../include/cpp_mediator/mediator.hpp"

#include <iostream>

// Define a request.
struct SayHello : holden::request<void> {
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
struct SayGoodbye {
    // Let mediator know what return type to infer.
    using response_type = FirstImpression;

    // the output stream through which the handler should "say hello."
    std::ostream& output_stream;

    SayGoodbye(std::ostream& out) : output_stream(out) {}
};

// Configure a request handlers.

class SpeakerHello
    : public holden::request_handler<SayHello> {
public:
    // Say hello on the provided output stream.
    std::shared_ptr<void> handle(SayHello& r) override {
        r.output_stream << "Hello, world!\n";
        return nullptr;
    }

};

class SpeakerGoodbye
    : public holden::request_handler<SayGoodbye> {
public:

    // Say goodbye on the provided output stream, and return how the interaction 
    // went.
    std::shared_ptr<FirstImpression> handle(SayGoodbye& r) override {
        r.output_stream << "Goodbye for now!\n";
        return std::make_shared<FirstImpression>(FirstImpression::Stupendous);
    }
};



int main() {
    holden::mediator mediator;

    auto sh = std::shared_ptr<SpeakerHello>(new SpeakerHello());
    auto sg = std::shared_ptr<SpeakerGoodbye>(new SpeakerGoodbye());
    
    mediator.add_handler(sh);
    mediator.add_handler(sg);

    SayHello say_hello{ std::cout };
    mediator.send(say_hello);

    SayGoodbye say_goodbye{ std::cout };
    auto first_impression = mediator.send(say_goodbye);

    std::cout << "The speaker's first impression was " << *first_impression << "\n";

    return 0;
}