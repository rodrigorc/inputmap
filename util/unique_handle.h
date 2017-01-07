#ifndef UNIQUE_HANDLE_H_INCLUDED
#define UNIQUE_HANDLE_H_INCLUDED

#include <memory>

template <typename T, T TNul = T()>
class UniqueHandle
{
public:
    UniqueHandle(std::nullptr_t = nullptr) noexcept
        :m_id(TNul)
    { }
    UniqueHandle(T x) noexcept
        :m_id(x)
    { }
    //T &operator*() { return m_id; }
    explicit operator bool() const  noexcept { return m_id != TNul; }

    operator T&() noexcept { return m_id; }
    operator T() const noexcept { return m_id; }

    T *operator&() noexcept { return &m_id; }
    const T *operator&() const noexcept { return &m_id; }

    friend bool operator == (UniqueHandle a, UniqueHandle b) noexcept { return a.m_id == b.m_id; }
    friend bool operator != (UniqueHandle a, UniqueHandle b) noexcept { return a.m_id != b.m_id; }
    friend bool operator == (UniqueHandle a, std::nullptr_t) noexcept { return a.m_id == TNul; }
    friend bool operator != (UniqueHandle a, std::nullptr_t) noexcept { return a.m_id != TNul; }
    friend bool operator == (std::nullptr_t, UniqueHandle b) noexcept { return TNul == b.m_id; }
    friend bool operator != (std::nullptr_t, UniqueHandle b) noexcept { return TNul != b.m_id; }

private:
    T m_id;
};

/* Example of use:

struct FooDeleter
{
    typedef UniqueHandle<Foo, FOO_NUL> pointer;
    void operator()(pointer p) noexcept
    {
        foo_free(p);
    }
};
typedef std::unique_ptr<Foo, FooDeleter> Buffer;
*/

#endif /* UNIQUE_HANDLE_H_INCLUDED */
