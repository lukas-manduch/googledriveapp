#pragma once

class ScopeExitImplBase
{
public:
    void Dismiss() const 
    {
        m_dismissed = true;
    }

    ScopeExitImplBase() = default;

    ScopeExitImplBase(const ScopeExitImplBase&) = delete;
    ScopeExitImplBase& operator=(const ScopeExitImplBase&) = delete;

    ScopeExitImplBase& operator=(ScopeExitImplBase&& rhs)
    {
        if (&rhs != this)
        {
            m_dismissed = rhs.m_dismissed;
            rhs.Dismiss();
        }
    }
protected:
    ~ScopeExitImplBase() {}

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
class ScopeExitImpl : public ScopeExitImplBase
{
    Lambda m_fun;
public:
    ScopeExitImpl(Lambda fun) : m_fun(fun) {}
    ~ScopeExitImpl() { SafeExecute(*this); } 

    ScopeExitImpl(const ScopeExitImpl&) = delete;
    ScopeExitImpl& operator=(const ScopeExitImpl&) = delete;

    ScopeExitImpl(ScopeExitImpl&& other)
    {
        *this = std::move(other);
    }

    ScopeExitImpl& operator=(ScopeExitImpl&& other)
    {
        if (&other != this)
        {
            m_fun = std::move(other.m_fun);
            (*static_cast<ScopeExitImplBase*>(this)) = std::move(static_cast<ScopeExitImplBase>(other));
        }
    }

    void Execute() { m_fun(); }
};

//	------------------------------------------------------------------------
template <typename Fun>
ScopeExitImpl<Fun> MakeGuard(Fun fun)
{
    return ScopeExitImpl<Fun>(fun);
}

typedef const ScopeExitImplBase& ScopeExit;


#define TOKEN_PASTE2(x, y) x ## y
#define TOKEN_PASTE(x, y) TOKEN_PASTE2(x, y)

#define ON_SCOPE_EXIT_INTERNAL1(lname, aname, ...) \
    auto lname = [&]() { __VA_ARGS__; }; \
    ScopeExit aname = MakeGuard(lname);

#define ON_SCOPE_EXIT_INTERNAL2(ctr, ...) \
    ON_SCOPE_EXIT_INTERNAL1(TOKEN_PASTE(Auto_func_, ctr), \
                   TOKEN_PASTE(Auto_instance_, ctr), __VA_ARGS__)

#define ON_SCOPE_EXIT(...) ON_SCOPE_EXIT_INTERNAL2(__LINE__, __VA_ARGS__)
