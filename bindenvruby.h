#pragma once

#ifndef __cplusplus
#error "C++ Only"
#endif

#ifndef BINDER_RUBY_TYPE_NOCHECK
#define BINDER_RUBY_TYPE_CHECK
#endif

#ifndef BINDER_RUBY_NUMBER_NOCHECK
#define BINDER_RUBY_NUMBER_CHECK
#endif

// C
#include <cassert>

// mruby
#include "mruby.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/variable.h"
#include "mruby/string.h"
// C++
#include <tuple>

// binder namespace
namespace BindER {
    // raise helper
    struct raise_helper {
#ifdef BINDER_RUBY_NUMBER_CHECK
        // raise
        template<size_t nparam>
        static void raisenarg(mrb_state *mrb, int narg) {
            if (narg != int(nparam)) {
                ::mrb_raisef(mrb, E_ARGUMENT_ERROR, "wrong number of arguments (%S for %S)",
                    mrb_fixnum_value(narg),
                    mrb_fixnum_value(int(nparam))
                );
            }
        }
#else
        // raise
        template<size_t> 
        static void raisenarg(mrb_state *, int) { }
#endif
    };
    // return original parameter/argument
    template<size_t id> struct original_parameter {};
    // helper for data object alloc
    template<typename T> 
    auto mrb_data_object_alloc_helper(mrb_state* ms, void*data) noexcept {
        return ::mrb_data_object_alloc(ms, 
            data_type_helper<T>::get_type().classr,
            data,
            &data_type_helper<T>::get_type().mrb_dt
        );
    }
    // helper for data type
    template<typename T> struct data_type_helper {
        // data_type
        struct data_type { mrb_data_type mrb_dt; RClass* classr; };
        // get uni data type
        static auto& get_type() noexcept {
            static data_type datatype;
            return datatype;
        }
        // init it
        static void set_type(const char* name, RClass* classr) noexcept {
            data_type_helper<T>::get_type().mrb_dt = {
                name, [](mrb_state* mrb, void* ptr) { 
                    (void)mrb; 
                    if (ptr) delete static_cast<T*>(ptr);
                }
            };
            data_type_helper<T>::get_type().classr = classr;
        }
    };
    // to match ruby-style, use low-case char
    // type helper with c++ tuple
    template <typename T> struct type_helper : public type_helper<decltype(&T::operator())> {};
    // type helper
    template <typename ClassType, typename ReturnType, typename... Args>
    struct type_helper<ReturnType(ClassType::*)(Args...) const> {
        // number of arguments
        enum : size_t { arity = sizeof...(Args) };
        // return type
        using result_type = ReturnType;
        // arg type
        template <size_t i> struct arg { using type = typename std::tuple_element<i, std::tuple<Args...>>::type; };
    };
    // call c++ function
    template<size_t ArgNum> struct call_chain {
        // check
        template<typename TypeHelper, typename T, typename RubyArgType, typename... Args>
        static auto check(T lam, RubyArgType* list, Args&&... args) noexcept { 
            constexpr size_t NEXT = ArgNum - 1;
            constexpr size_t INDEX = TypeHelper::arity - ArgNum;
            using parma_type = TypeHelper::arg<INDEX>::type;
            return call_chain<NEXT>::check<TypeHelper>(
                lam, list, std::forward<Args>(args)..., ruby_arg<parma_type>::check(list[INDEX])
                );
        }
        // call
        template<typename TypeHelper, typename T, typename RubyArgType, typename... Args>
        static auto call(T lam, RubyArgType* list, Args&&... args) noexcept { 
            constexpr size_t NEXT = ArgNum - 1;
            constexpr size_t INDEX = TypeHelper::arity - ArgNum;
            using parma_type = TypeHelper::arg<INDEX>::type;
            return call_chain<NEXT>::call<TypeHelper>(
                lam, list, std::forward<Args>(args)..., ruby_arg<parma_type>::get(list[INDEX])
                );
        }
    };
    // base arg
    template<> struct call_chain<0> {
        // check
        template<typename TypeHelper, typename T, typename RubyArgType, typename... Args>
        static auto check(T lam, RubyArgType*, Args&&...args) noexcept { return lam(std::forward<Args>(args)...); }
        // call
        template<typename TypeHelper, typename T, typename RubyArgType, typename... Args>
        static auto call(T lam, RubyArgType*, Args&&...args) noexcept { return lam(std::forward<Args>(args)...); }
    };
    // type helper for pointer type to obj type
    template<typename T> struct type_helper_ptr { using type = T; };
    // type helper for pointer type to obj type
    template<typename T> struct type_helper_ptr<T*> { using type = typename type_helper_ptr<T>::type; };
    // ruby binder
    static inline auto ruby_binder() noexcept { assert(!"bad overload"); return 0ui32; };
    // ruby arg to c++
    template<typename T> struct ruby_arg { };
    // ruby arg to c++: for void
    template<> struct ruby_arg<void> {
        // get mruby
        static auto get(const mrb_value& /*value*/) noexcept { return; }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state*, Lam lam, const mrb_value* /*arg*/) noexcept { lam(); return ::mrb_nil_value(); }
    };
    // ruby arg to c++: for bool
    template<> struct ruby_arg<bool> {
        // get mruby
        static auto get(const mrb_value& v) noexcept { return mrb_test(v); }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state*, Lam lam, const mrb_value* /*arg*/) noexcept { 
            return ::mrb_bool_value(!!(lam())); 
        }
    };
    // mruby arg to c++: for float
    template<> struct ruby_arg<float> {
        // get
        static auto get(const mrb_value& v) noexcept { 
            return mrb_float_p(v) ? static_cast<float>(mrb_float(v)) : static_cast<float>(mrb_fixnum(v)); 
        }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state* ms, Lam lam, const mrb_value* /*arg*/) noexcept {
            return ::mrb_float_value(ms, static_cast<float>(lam()));
        }
    };
    // mruby arg to c++: for float
    template<> struct ruby_arg<double> {
        // get
        static auto get(const mrb_value& v) noexcept { 
            return mrb_float_p(v) ? static_cast<double>(mrb_float(v)) : static_cast<double>(mrb_fixnum(v)); 
        }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state* ms, Lam lam, const mrb_value* /*arg*/) noexcept {
            return ::mrb_float_value(ms, static_cast<double>(lam())); 
        }
    };
    // mruby arg to c++: for int32_t
    template<> struct ruby_arg<int32_t> {
        // get
        static auto get(const mrb_value& v) noexcept { 
            return mrb_fixnum_p(v) ?  static_cast<int32_t>(mrb_fixnum(v)) : static_cast<int32_t>(mrb_float(v)); 
        }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state*, Lam lam, const mrb_value* /*arg*/) noexcept { 
            return ::mrb_fixnum_value(lam()); 
        }
    };
    // mruby arg to c++: for uint32_t
    template<> struct ruby_arg<uint32_t> {
        // get
        static auto get(const mrb_value& v) noexcept { 
            return mrb_fixnum_p(v) ?  static_cast<uint32_t>(mrb_fixnum(v)) : static_cast<uint32_t>(mrb_float(v)); 
        }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state*, Lam lam, const mrb_value* /*arg*/) noexcept { 
            return ::mrb_fixnum_value(static_cast<uint32_t>(lam())); 
        }
    };
    // mruby arg to c++: for const char*
    template<> struct ruby_arg<const char*> {
        // get
        static auto get(const mrb_value& v) noexcept { return RSTRING_PTR(v); }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state* ms, Lam lam, const mrb_value* /*arg*/) noexcept { 
            return ::mrb_str_new_cstr(ms, lam());
        }
    };
    // mruby arg to c++: for void*
    template<> struct ruby_arg<void*> {
        // get
        static auto get(const mrb_value& v) noexcept { return mrb_cptr(v); }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state* ms, Lam lam, const mrb_value* /*arg*/) noexcept { 
            return ::mrb_cptr_value(ms, lam());
        }
    };
    // mruby arg to c++: for void*
    template<size_t id> struct ruby_arg<original_parameter<id>> {
        // get
        static auto get(const mrb_value& v) noexcept { static_assert(false, "error"); }
        // set mruby
        template<typename Lam>
        static auto set(mrb_state*, Lam lam, const mrb_value* arg) noexcept { 
            lam();  return arg[id];
        }
    };
    // -----------------------------------
    //      ADD YOUR OWN TYPE HERE        
    // -----------------------------------
    // mruby binder
    class mruby_binder {
    public:
        // class binder
        template<typename CppClass>
        class class_binder {
            // method helper
            template<size_t ArgNum> struct method_helper {
                // bind
                template<typename T> 
                static inline auto bind(const class_binder<CppClass>& binder, const char* method_name, T method) noexcept {
                    return method_helper_ex<type_helper<T>::arg<0>::type>::bind(binder, method_name, method);
                }
            };
            // method helper for zero arg
            template<> struct method_helper<0> {
                // bind
                template<typename T> 
                static inline auto bind(const class_binder<CppClass>& binder, const char* method_name, T method) noexcept {
                    // no-arg -> [rb]class-method call [cpp]static-class-function
                    // real ctor lambda to avoid capture
                    static const auto real_method(method);
                    // define
                    ::mrb_define_class_method(binder.get_mruby(), binder.get_class(), method_name, [](mrb_state* mrb, mrb_value self) noexcept {
                        (void)self;
                        return ruby_arg<type_helper<T>::result_type>::set(mrb, real_method, nullptr);
                    }, MRB_ARGS_NONE());
                }
            };
            // method helper ex
            template<typename First> struct method_helper_ex {
                // bind
                template<typename T> 
                static inline auto bind(const class_binder<CppClass>& binder, const char* method_name, T method) noexcept {
                    // no-arg -> [rb]class-method call [cpp]static-class-function
                    // real ctor lambda to avoid capture
                    static const auto real_method(method);
                    using traits = type_helper<T>;
                    // define
                    ::mrb_define_class_method(binder.get_mruby(), binder.get_class(), method_name, [](mrb_state* mrb, mrb_value self) noexcept {
                        (void)self;
                        mrb_value* args; int narg;
                        ::mrb_get_args(mrb, "*", &args, &narg);
                        // raise error for arg number
                        raise_helper::raisenarg<traits::arity>(mrb, narg);
                        // no arg call
                        auto no_arg_lambda = [args]() noexcept {
                            return call_chain<traits::arity>::call<traits>(real_method, args);
                        };
                        return ruby_arg<type_helper<T>::result_type>::set(mrb, no_arg_lambda, args);
                    }, MRB_ARGS_REQ(traits::arity));
                }
            };
            // method helper ex: for object-ptr
            template<> struct method_helper_ex<CppClass*> {
                // bind
                template<typename T> 
                static inline auto bind(const class_binder<CppClass>& binder, const char* method_name, T method) noexcept {
                    // no-arg -> [rb]class-method call [cpp]static-class-function
                    // real ctor lambda to avoid capture
                    static const auto real_method(method);
                    static const auto sc_method_name(method);
                    using traits = type_helper<T>;
                    // define
                    ::mrb_define_method(binder.get_mruby(), binder.get_class(), method_name, [](mrb_state* mrb, mrb_value self) noexcept {
                        auto obj = reinterpret_cast<CppClass*>(DATA_PTR(self));
                        mrb_value* args; int narg;
                        ::mrb_get_args(mrb, "*", &args, &narg);
                        // raise error for arg number
                        raise_helper::raisenarg<traits::arity - 1>(mrb, narg);
                        // no arg call
                        auto no_arg_lambda = [args, obj]() noexcept {
                            return call_chain<traits::arity-1>::call<traits>(real_method, args-1, obj);
                        };
                        return ruby_arg<type_helper<T>::result_type>::set(mrb, no_arg_lambda, args);
                    }, MRB_ARGS_REQ(traits::arity));
                }
            };
            // method helper ex: for const object-pointer
            template<> struct method_helper_ex<const CppClass*> : method_helper_ex<CppClass*> { };
        public:
            // get class
            static inline auto get_class() noexcept { return data_type_helper<CppClass>::get_type().classr; }
            // get mruby
            auto get_mruby() const noexcept { return mstate; }
            // ctor
            class_binder(mrb_state* s) noexcept : mstate(s) { assert(mstate && "bad argument"); };
            // copy ctor
            class_binder(const class_binder<CppClass>& b) noexcept : mstate(b.mstate) { assert(mstate && "bad argument"); };
            // bind
            template<typename T> auto bind(const char* method_name, T method) {
#ifdef _DEBUG
                static bool isfirst = true;
                assert(isfirst && "cannot call twice with same template");
                isfirst = false;
#endif
                // helper
                using traits = type_helper<T>;
                method_helper<traits::arity>::bind(*this, method_name, method);
            }
        private:
            // state of mruby
            mrb_state*      mstate = nullptr;
        };
    public:
        // ctor
        mruby_binder(mrb_state* state) noexcept : mstate(state) { assert(mstate && "bad argument"); };
        // copy ctor
        mruby_binder(const mruby_binder& b) noexcept : mstate(b.mstate) { assert(mstate && "bad argument"); };
        // bind module
        template<typename T>
        inline auto bind_module (const char* module_name) noexcept {
            return this->bind_module<T>(module_name, mstate->kernel_module);
        }
        // bind module with outer
        template<typename T>
        inline auto bind_module(const char* module_name, RClass* outer) noexcept {
            // define class
            auto cla = ::mrb_define_module_under(mstate, outer, module_name);
            return class_binder<T>(mstate, cla);
        }
        // bind class
        template<typename T> inline auto bind_class(const char* class_name, T ctor) noexcept {
            return this->bind_class(class_name, ctor, mstate->kernel_module, mstate->object_class);
        }
        // bind class with outer and super
        template<typename T> inline auto bind_class(const char* class_name, T ctor, RClass* outer, RClass* super) noexcept {
#ifdef _DEBUG
            static bool isfirst = true;
            assert(isfirst && "cannot call twice with same template");
            isfirst = false;
#endif
            // define class
            auto cla = ::mrb_define_class_under(mstate, outer, class_name, super);
            assert(cla && "error from mruby or bad action");
            MRB_SET_INSTANCE_TT(cla, MRB_TT_DATA);
            using traits = type_helper<T>;
            using class_type = type_helper_ptr<traits::result_type>::type;
            // data type
            data_type_helper<class_type>::set_type(class_name, cla);
            // real ctor lambda to avoid capture
            static const auto real_ctor(ctor);
            // define initialize method
            auto initialize_this = [](mrb_state *mrb, mrb_value self) noexcept {
                DATA_TYPE(self) = &data_type_helper<class_type>::get_type().mrb_dt;
                mrb_value* args; int narg;
                ::mrb_get_args(mrb, "*", &args, &narg);
                // raise error for arg number
                raise_helper::raisenarg<traits::arity>(mrb, narg);
                //assert(narg == traits::arity && "bad arguments");
                DATA_PTR(self) = call_chain<traits::arity>::call<traits>(real_ctor, args);
                return self;
            };
            ::mrb_define_method(mstate, cla, "initialize", initialize_this, MRB_ARGS_REQ(traits::arity));
            return class_binder<class_type>(mstate);
        }
    private:
        // state of mruby
        mrb_state*              mstate = nullptr;
    };
    // overload for mruby binder
    static inline auto ruby_binder(mrb_state* data) noexcept { return mruby_binder(data); }
}