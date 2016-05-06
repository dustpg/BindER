## BindER -- Binder Environment for Ruby with C++
  - C++14
  - bind support for **mruby only** yet.
  - just put 'bindenvruby.h' to your project and include it
   
## Easy Guide  
- mruby
```cpp
// call binder
auto binder_mruby(mrb_state* mruby) {
    int random_data = 0;
    auto binder = BindER::ruby_binder(mruby);
    {
        // ctor
        auto foobinder = binder.bind_class("Foo", [](int a, int b) {
            return new(std::nothrow) Foo(a+b);
        });
        // first argument is binded-class pointer -> instance-method
        foobinder.bind("bar", [](Foo* obj, int a, int b, int c) noexcept { return obj->bar(a, b, c); });
        // first argument NOT binded-class pointer -> class-method
        foobinder.bind("baz", [](int b) noexcept { return Foo::baz(b, 5, 7); });
        // limited lambda working(do not capture value-passed-custom-type-object because of static lambda)
        foobinder.bind("baa", [&]() noexcept { return Foo::baz(random_data, 5, 7); });
        // // return first parameter given
        foobinder.bind("foo=", [](Foo* obj, Foo2* f2) noexcept { 
            obj->set_foo(f2); return BindER::original_parameter<0>();
        });
    }
}
```
- more codes, refer to files under folder `test`

## Performance(In MSC)
[when noinline](./__resource/noinline_debug_vs2015.png) [when inline](./__resource/inline_release_vs2015.png)  

## Custom Type Support
  - search `ADD YOUR OWN TYPE HERE`
  - add your own type
  - for example with `std::string`  
  
  ```cpp
    // mruby arg to c++: for std::string
    template<> struct ruby_arg<std::string> {
        // get, hope RVO working
        static auto get(const mrb_value& v) noexcept { return std::string(RSTRING_PTR(v)); }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state* ms, Lam lam) noexcept { return ::mrb_str_new_cstr(ms, lam().c_str()); }
    };
    // mruby arg to c++: for std::string&, same with std::string
    template<> struct ruby_arg<std::string&> :  ruby_arg<std::string> { };
    // mruby arg to c++: for const std::string&, same with std::string&
    template<> struct ruby_arg<const std::string&> :  ruby_arg<std::string&> { };
  ```
  
## License  
  MIT or WTFPL, refer to [License-MIT.txt](./License-MIT.txt) or [License-WTFPL.txt](./License-WTFPL.txt) please