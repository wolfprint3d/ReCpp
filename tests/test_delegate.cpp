#define _DEBUG_FUNCTIONAL_MACHINERY
#include <rpp/delegate.h>
#include <rpp/stack_trace.h>
#include <rpp/tests.h>
using namespace rpp;


// a generic data container for testing instances, functors and lambdas
class Data
{
public:
    char* data = nullptr;
    static char* alloc(const char* str) {
        size_t n = strlen(str) + 1;
        return (char*)memcpy(new char[n], str, n);
    }
    explicit Data(const char* s) : data(alloc(s)) {}
    Data()                       : data(alloc("data")) {}
    Data(const Data& d)          : data(alloc(d.data)) {}
    Data(Data&& d) noexcept      : data(d.data) { d.data = nullptr; }
    ~Data() { delete[] data; }
    Data& operator=(Data&& d) noexcept {
        std::swap(data, d.data);
        return *this;
    }
    Data& operator=(const Data& d) {
        if (this != &d) {
            delete[] data;
            data = alloc(d.data);
        }
        return *this;
    }
    bool operator==(const char* s) const { return strcmp(data, s) == 0; }
    bool operator!=(const char* s) const { return strcmp(data, s) != 0; }
};
string to_string(const Data& d) { return d.data; }

#define __validate_data_arg(arg) \
    if (!arg.data || arg != "data") \
        throw traced_exception("Argument `"#arg"` did not contain \"data\"");

static Data validate(const char* name, const Data& a)
{
    printf("%s: '%s'\n", name, a.data);
    __validate_data_arg(a);
    return Data{name};
}
static Data validate(const char* name, const Data& a, const Data& b)
{
    printf("%s: '%s' '%s'\n", name, a.data, b.data);
    __validate_data_arg(a);
    __validate_data_arg(b);
    return Data{name};
}
//static Data validate(const char* name, const Data& a, const Data& b, const Data& c)
//{
//    printf("%s: '%s' '%s' '%s'\n", name, a.data, b.data, c.data);
//    __validate_data_arg(a);
//    __validate_data_arg(b);
//    __validate_data_arg(c);
//    return Data{name};
//}
static Data validate(const char* name, const Data& a, const Data& b, const Data& c, const Data& d)
{
    printf("%s: '%s' '%s' '%s' '%s'\n", name, a.data, b.data, c.data, d.data);
    __validate_data_arg(a);
    __validate_data_arg(b);
    __validate_data_arg(c);
    __validate_data_arg(d);
    return Data{name};
}


TestImpl(test_delegate)
{
    using DataDelegate = delegate<Data(Data a)>;
    Data data;

    TestInit(test_delegate)
    {
        //#if __GNUG__
        //		BaseClass a;
        //		typedef int(*func_type)(BaseClass* a, int i);
        //		int (BaseClass::*membfunc)(int i) = &BaseClass::func3;
        //		func_type fp = (func_type)(&a->*membfunc);
        //		printf("BaseClass::func3 %p\n", fp);
        //#endif
    }

    ////////////////////////////////////////////////////

    TestCase(functions)
    {
        Data (*function)(Data a) = [](Data a)
        {
            return validate("function", a);
        };

        DataDelegate func = function;
        AssertThat((bool)func, true);
        AssertThat(func(data), "function");
    }

    ////////////////////////////////////////////////////

    struct Base
    {
        Data x;
        virtual ~Base(){}
        Data method(Data a)
        {
            return validate("method", a, x);
        }
        Data const_method(Data a) const
        {
            return validate("const_method", a, x);
        }
        virtual Data virtual_method(Data a)
        {
            return validate("virtual_method", a, x);
        }
    };
    struct Derived : Base
    {
        Data virtual_method(Data a) override
        {
            return validate("derived_method", a, x);
        }
    };

    TestCase(methods_bug)
    {
        using memb_type = Data (*)(void*, Data);
        struct dummy {};
        using dummy_type = Data (dummy::*)(Data a);
        union method_helper
        {
            memb_type mfunc;
            dummy_type dfunc;
        };

        Base inst;
        Data (Base::*method)(Data a) = &Base::method;

        void* obj = &inst;
        //printf("obj:  %p\n", obj);

        method_helper u;
        u.dfunc = (dummy_type)method;

        dummy* dum = (dummy*)obj;
        (dum->*u.dfunc)(data);
    }

    TestCase(methods)
    {
        Derived inst;
        DataDelegate func1(inst, &Derived::method);
        AssertThat(func1(data), "method");

        DataDelegate func2(inst, &Derived::const_method);
        AssertThat(func2(data), "const_method");
    }

    TestCase(virtuals)
    {
        Base    base;
        Derived inst;

        DataDelegate func1(base, &Base::virtual_method);
        AssertThat(func1(data), "virtual_method");

        DataDelegate func2((Base*)&inst, &Base::virtual_method);
        AssertThat(func2(data), "derived_method");

        DataDelegate func3(inst, &Derived::virtual_method);
        AssertThat(func3(data), "derived_method");
    }

    ////////////////////////////////////////////////////

    TestCase(lambdas)
    {
        DataDelegate lambda1 = [](Data a)
        {
            return validate("lambda1", a);
        };
        AssertThat(lambda1(data), "lambda1");

        DataDelegate lambda2 = DataDelegate([x=data](Data a)
        {
            return validate("lambda2", a, x);
        });
        AssertThat(lambda2(data), "lambda2");
    }

    TestCase(nested_lambdas)
    {
        DataDelegate lambda = [x=data](Data a)
        {
            DataDelegate nested = [x=x](Data a)
            {
                return validate("nested_lambda", x);
            };
            return nested(a);
        };
        AssertThat(lambda(data), "nested_lambda");

        DataDelegate moved_lambda = move(lambda);
        AssertThat((bool)lambda, false);
        AssertThat(moved_lambda(data), "nested_lambda");
    }

    TestCase(functor)
    {
        struct Functor
        {
            Data x;
            Data operator()(Data a) const
            {
                return validate("functor", a, x);
            }
        };

        DataDelegate func = Functor();
        AssertThat(func(data), "functor");
    }

    TestCase(move_init)
    {
        DataDelegate lambda = [x=data](Data a)
        {
            return validate("move_init", a);  
        };

        DataDelegate init { move(lambda) };
        AssertThat((bool)init, true);
        AssertThat((bool)lambda, false);
        AssertThat(init(data), "move_init");
    }

    ////////////////////////////////////////////////////

    static void event_func(Data a)
    {
        validate("event_func", a);
    }

    TestCase(multicast_delegates)
    {
        struct Receiver
        {
            Data x;
            void event_method(Data a)
            {
                validate("event_method", a, x);
            }
            void const_method(Data a) const
            {
                validate("const_method", a, x);
            }
            void unused_method(Data a) const { const_method(a); }
        };

        Receiver receiver;
        multicast_delegate<Data> evt;
        Assert(evt.size() == 0); // yeah...

        // add 2 events
        evt += &event_func;
        evt.add(receiver, &Receiver::event_method);
        evt.add(receiver, &Receiver::const_method);
        evt(data);
        Assert(evt.size() == 3);

        // remove one event
        evt -= &event_func;
        evt(data);
        Assert(evt.size() == 2);

        // try to remove an incorrect function:
        evt -= &event_func;
        Assert(evt.size() == 2); // nothing must change
        evt.remove(receiver, &Receiver::unused_method);
        Assert(evt.size() == 2); // nothing must change

        // remove final events
        evt.remove(receiver, &Receiver::event_method);
        evt.remove(receiver, &Receiver::const_method);
        evt(data);
        AssertThat(evt.size(), 0); // must be empty now
        AssertThat(evt.empty(), true);
        AssertThat((bool)evt, false);
    }

    TestCase(multicast_delegate_copy_and_move)
    {
        int count = 0;
        multicast_delegate<Data> evt;
        evt += [&](Data a)
        {
            ++count;
            validate("evt1", a); 
        };
        evt += [&](Data a)
        {
            ++count;
            validate("evt2", a);
        };
        AssertThat(evt.empty(), false);
        AssertThat((bool)evt, true);
        AssertThat(evt.size(), 2);
        evt(data);
        AssertThat(count, 2);

        count = 0;
        multicast_delegate<Data> evt2 = evt;
        AssertThat(evt2.empty(), false);
        AssertThat((bool)evt2, true);
        AssertThat(evt2.size(), 2);
        evt2(data);
        AssertThat(count, 2);

        count = 0;
        multicast_delegate<Data> evt3 = move(evt2);
        AssertThat(evt3.empty(), false);
        AssertThat((bool)evt3, true);
        AssertThat(evt3.size(), 2);
        evt3(data);
        AssertThat(count, 2);
    }

    TestCase(std_function_args)
    {
        std::function<void(Data, Data&, const Data&, Data&&)> fun =
            [&](Data a, Data& b, const Data& c, Data&& d)
        {
            validate("stdfun", a, b, c, d); 
        };
        Data copy = data;
        fun.operator()(data, data, data, std::move(copy));
    }

    TestCase(multicast_delegate_mixed_reference_args)
    {
        int count = 0;
        multicast_delegate<Data, Data&, const Data&, Data&&> evt;
        evt += [&](Data a, Data& b, const Data& c, Data&& d)
        {
            ++count;
            validate("evt1", a, b, c, d); 
        };
        evt += [&](Data a, Data& b, const Data& c, Data&& d)
        {
            ++count;
            validate("evt2", a, b, c, d); 
        };
        AssertThat(evt.empty(), false);
        AssertThat((bool)evt, true);
        AssertThat(evt.size(), 2);

        Data copy = data;
        evt(data, data, data, std::move(copy));
        AssertThat(count, 2);
    }

    ////////////////////////////////////////////////////

};
