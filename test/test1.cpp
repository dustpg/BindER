#include "../bindenvruby.h"
#include <mruby/compile.h>

#ifdef _MSC_
__declspec(noinline) 
#endif
auto noinline_check() {
    std::printf("");
}

class Foo {
public:
    Foo(int a) :data(size_t(a)) { std::printf("[%s] - %d\r\n", __FUNCTION__, a); }
    ~Foo() { std::printf("[%s]\r\n", __FUNCTION__); }
public:
    // bar
    auto bar(int a , int b, int c) const {
        noinline_check(); printf("[%s]%d ? %d\r\n", __FUNCTION__, a, a*b + c); return 0.f;
    }// bar
    static auto baz(int a , int b , int c) {
        noinline_check();  printf("[%s]%d ? %d\r\n", __FUNCTION__, a, a*b + c); return 987;
    }
private:
    // data
    size_t data = 0;
};


class Foo2 {
public:
    Foo2() { std::printf("[%s]\r\n", __FUNCTION__); }
    ~Foo2() { std::printf("[%s]\r\n", __FUNCTION__); }
private:
    // data
    size_t data = 0;
};

// ruby script
static const char* const rbscript = u8R"rb(
puts "Hello, World!"
a = 0
a = Foo.new 0, 1
p Foo.baa.class
p Foo.baz(9).class
p a.bar 1,2,3
a = Foo2.new "sad", 120, 0.0, 0, 1, 3.1416
)rb";

// call binder
auto binder_mruby(mrb_state* mruby) {
    int random_data = 0;
    auto binder = BindER::ruby_binder(mruby);
    {
        // ctor
        auto classbinder = binder.bind_class("Foo", [](int a, int b) {
            return new(std::nothrow) Foo(a+b);
        });
        // first argument is binded-class pointer/reference -> instance-method
        classbinder.bind("bar", [](Foo& obj, int a, int b, int c) noexcept { return obj.bar(a, b, c); });
        // first argument not binded-class pointer -> class-method
        classbinder.bind("baz", [](int b) noexcept { return Foo::baz(b, 5, 7); });
        // limited lambda working(do not capture value-passed-object because of static lambda)
        classbinder.bind("baa", [&]() noexcept { return Foo::baz(random_data, 5, 7); });
    }
    {
        auto classbinder = binder.bind_class("Foo2", [](const char* v, int32_t a, float b, int32_t c, int32_t d, float f) {
            return new(std::nothrow) Foo2();
        });
    }
}


// main
int main(int argc, char* argv[]) {
    auto mruby = ::mrb_open();
    if (mruby) {
        binder_mruby(mruby);
        ::mrb_load_string(mruby, rbscript);
        ::mrb_close(mruby);
        mruby = nullptr;
    }
    std::getchar();
    return 0;
}

