#pragma once

#include <typeindex>
#include <typeinfo>
#include <utility>
#include <type_traits>

#include "Core.h"

_WHIP_START

class Payload {
public:
    Payload() = default;

    Payload(const Payload&) = delete;
    Payload& operator=(const Payload&) = delete;

    Payload(Payload&& other) noexcept {
        MoveFrom(std::move(other));
    }

    Payload& operator=(Payload&& other) noexcept {
        if (this != &other) {
            Reset();
            MoveFrom(std::move(other));
        }

        return *this;
    }

    ~Payload() {
        Reset();
    }

    template<typename T>
    static Payload Ref(T& value) {
        return Payload(&value, typeid(T), nullptr);
    }

    template<typename T, typename... Args>
    static Payload Make(Args&&... args) {
        T* value = new T(std::forward<Args>(args)...);

        return Payload(
            value,
            typeid(T),
            [](void* ptr) {
                delete static_cast<T*>(ptr);
            }
        );
    }

	static Payload Null() {
	    return Payload{};
    }

    template<typename T>
    T* As() {
        if (!m_Ptr || m_Type != std::type_index(typeid(T))) {
            return nullptr;
        }

        return static_cast<T*>(m_Ptr);
    }

    template<typename T>
    const T* As() const {
        if (!m_Ptr || m_Type != std::type_index(typeid(T))) {
            return nullptr;
        }

        return static_cast<const T*>(m_Ptr);
    }

    template<typename T>
    T& Get() {
        T* value = As<T>();

        if (!value) {
            throw std::bad_cast();
        }

        return *value;
    }

    template<typename T>
    const T& Get() const {
        const T* value = As<T>();

        if (!value) {
            throw std::bad_cast();
        }

        return *value;
    }

    template<typename T>
    T& GetOr(T& fallback) {
        T* value = As<T>();

        if (!value) {
            return fallback;
        }

        return *value;
    }

    template<typename T>
    const T& GetOr(std::reference_wrapper<const T> fallback) const {
        const T* value = As<T>();

        if (!value) {
            return fallback.get();
        }

        return *value;
    }

    template<typename T>
    T& GetOr(const T&& fallback) = delete;

    template<typename T>
    const T& GetOr(const T&& fallback) const = delete;

    template<typename T>
    T GetOrValue(T fallback) const {
        const T* value = As<T>();

        if (!value) {
            return fallback;
        }

        return *value;
    }

    void* Raw() {
        return m_Ptr;
    }

    const void* Raw() const {
        return m_Ptr;
    }

    bool Empty() const {
        return m_Ptr == nullptr;
    }

    void Reset() {
        if (m_Ptr && m_Deleter) {
            m_Deleter(m_Ptr);
        }

        m_Ptr = nullptr;
        m_Deleter = nullptr;
        m_Type = typeid(void);
    }

private:
    using DeleterFn = void(*)(void*);

    Payload(void* ptr, std::type_index type, DeleterFn deleter)
        : m_Ptr(ptr), m_Type(type), m_Deleter(deleter) {}

    void MoveFrom(Payload&& other) noexcept { // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        m_Ptr = other.m_Ptr;
        m_Type = other.m_Type;
        m_Deleter = other.m_Deleter;

        other.m_Ptr = nullptr;
        other.m_Type = typeid(void);
        other.m_Deleter = nullptr;
    }

private:
    void* m_Ptr = nullptr;
    std::type_index m_Type = typeid(void);
    DeleterFn m_Deleter = nullptr;
};

_WHIP_END
