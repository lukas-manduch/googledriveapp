#pragma once


class ScopeGuardImplBase
{
public:
    void Dismiss() const 
    {
        m_dismissed = true;
    }

    ScopeGuardImplBase() = default;

    ScopeGuardImplBase(const ScopeGuardImplBase&) = delete;
    ScopeGuardImplBase& operator=(const ScopeGuardImplBase&) = delete;

    ScopeGuardImplBase& operator=(ScopeGuardImplBase&& other)
    {
        if (&other != this)
        {
            m_dismissed = other.m_dismissed;
            other.Dismiss();
        }
    }
protected:
    ~ScopeGuardImplBase() {}

    template <typename J>
    static void SafeExecute(J& j)
    {
        if (!j.m_dismissed)
        {
            j.Execute();
        }
    }

    mutable bool m_dismissed = false;
};

template <typename Lambda>
class ScopeGuardImpl : public ScopeGuardImplBase
{
    Lambda m_fun;
public:
    ScopeGuardImpl(Lambda fun) : m_fun(fun) {}
    ~ScopeGuardImpl() { SafeExecute(*this); } 

    ScopeGuardImpl(const ScopeGuardImpl&) = delete;
    ScopeGuardImpl& operator=(const ScopeGuardImpl&) = delete;

    ScopeGuardImpl& operator=(ScopeGuardImpl&& other)
    {
        if (&other != this)
        {
            m_fun = std::move(other.m_fun);
            (*static_cast<ScopeGuardImplBase*>(this)) = static_cast<ScopeGuardImplBase>(other);
        }
    }

    void Execute() { m_fun(); }
};

template <typename Fun>
ScopeGuardImpl<Fun> MakeGuard(Fun fun)
{
    return ScopeGuardImpl<Fun>(fun);
}

typedef const ScopeGuardImplBase& ScopeGuard;

/*
#define TOKEN_PASTEx(x, y) x ## y
#define TOKEN_PASTE(x, y) TOKEN_PASTEx(x, y)

#define Auto_INTERNAL1(lname, aname, ...) \
    auto lname = [&]() { __VA_ARGS__; }; \
    AtScopeExit<decltype(lname)> aname(lname);

#define Auto_INTERNAL2(ctr, ...) \
    Auto_INTERNAL1(TOKEN_PASTE(Auto_func_, ctr), \
                   TOKEN_PASTE(Auto_instance_, ctr), __VA_ARGS__)

#define Auto(...) Auto_INTERNAL2(__LINE__, __VA_ARGS__)
*/