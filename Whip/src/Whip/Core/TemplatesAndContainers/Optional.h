#pragma once

#include <stdexcept>
#include <type_traits>

#include <Whip/Core/Core.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>
#include <Whip/Core/TemplatesAndContainers/Utility.h>

_WHIP_START

struct nullopt_t
{
	struct init_tag {};
	constexpr explicit nullopt_t(init_tag) {}
};

WHP_INLINE constexpr nullopt_t nullopt{ nullopt_t::init_tag{} };

class bad_optional_access : public std::logic_error
{
public:
	explicit bad_optional_access() : logic_error("bad optional access") {}
};

template <class _Ty>
class optional
{
public:
	using value_type = std::remove_const_t<_Ty>;

	optional() noexcept : m_has_value(false), m_value() {}

	optional(nullopt_t) noexcept : m_has_value(false), m_value() {}

	optional(const _Ty& value) noexcept : m_has_value(true), m_value(value) {}

	optional(const optional& right) noexcept : m_value()
	{
		m_has_value = right.m_has_value;
		if (right.m_has_value)
			m_value = right.m_value;
	}

	optional& operator=(nullopt_t) noexcept
	{
		reset();
		return *this;
	}

	optional& operator=(const _Ty& value) noexcept
	{
		initialize(value);
		return *this;
	}

	optional& operator=(const optional& right) noexcept
	{
		m_has_value = right.m_has_value;
		if (right.m_has_value)
			m_value = right.m_value;
	}

	explicit operator bool() const
	{
		return m_has_value;
	}

	bool has_value() const noexcept
	{
		return m_has_value;
	}

	value_type& value()
	{
		if (!m_has_value)
			throw_boa();
		return m_value;
	}

	const value_type& value() const
	{
		if (!m_has_value)
			throw_boa();
		return m_value;
	}

	template <class _Uty, std::enable_if_t<std::is_convertible_v<_Uty, _Ty>, int> = 0>
	value_type value_or(const _Uty& other_value)
	{
		return m_has_value ? m_value : static_cast<_Ty>(other_value);
	}

	void reset() noexcept
	{
		m_has_value = false;
	}

	value_type* operator->()
	{
		WHP_CORE_ASSERT(m_has_value, "optional has no value.");
		return &m_value;
	}

	const value_type* operator->() const
	{
		WHP_CORE_ASSERT(m_has_value, "optional has no value.");
		return &m_value;
	}

	value_type& operator*()
	{
		WHP_CORE_ASSERT(m_has_value, "optional has no value.");
		return m_value;
	}

	const value_type& operator*() const
	{
		WHP_CORE_ASSERT(m_has_value, "optional has no value.");
		return m_value;
	}

	void swap(optional& rhs)
	{
		if (this != addressof(rhs))
		{
			if (!m_has_value && !rhs.m_has_value)
				return;
			if (m_has_value && rhs.m_has_value)
				_WHIP swap(**this, *rhs);
			else if (m_has_value && !rhs.m_has_value)
			{
				rhs.initialize(m_value);
				reset();
			}
			else if (!m_has_value && rhs.m_has_value)
			{
				initialize(rhs.m_value);
				rhs.reset();
			}
		}
	}

private:
	template <typename _Uty, enable_if_t<is_convertible_v<_Uty, _Ty>, int> = 0>
	void initialize(const _Uty& value)
	{
		WHP_CORE_ASSERT(m_has_value, "optional couldn't initialize.");
		m_value = static_cast<_Ty>(value);
		m_has_value = true;
	}

	void throw_boa()
	{
		throw bad_optional_access{};
	}
private:
	bool m_has_value;
	value_type m_value;
};

template<typename _Ty, typename _Uty>
inline bool operator==(optional<_Ty> const& x, optional<_Uty> const& y)
{
	return bool(x) != bool(y) ? false : bool(x) == false ? true : *x == *y;
}

template<typename _Ty, typename _Uty>
inline bool operator!=(optional<_Ty> const& x, optional<_Uty> const& y)
{
	return !(x == y);
}

template<typename _Ty, typename _Uty>
inline bool operator<(optional<_Ty> const& x, optional<_Uty> const& y)
{
	return (!y) ? false : (!x) ? true : *x < *y;
}

template<typename _Ty, typename _Uty>
inline bool operator>(optional<_Ty> const& x, optional<_Uty> const& y)
{
	return (y < x);
}

template<typename _Ty, typename _Uty>
inline bool operator<=(optional<_Ty> const& x, optional<_Uty> const& y)
{
	return !(y < x);
}

template<typename _Ty, typename _Uty>
inline bool operator>=(optional<_Ty> const& x, optional<_Uty> const& y)
{
	return !(x < y);
}

// Comparison with nullopt

template<typename _Ty>
inline bool operator==(optional<_Ty> const& x, nullopt_t)
{
	return (!x);
}

template<typename _Ty>
inline bool operator==(nullopt_t, optional<_Ty> const& x)
{
	return (!x);
}

template<typename _Ty>
inline bool operator!=(optional<_Ty> const& x, nullopt_t)
{
	return bool(x);
}

template<typename _Ty>
inline bool operator!=(nullopt_t, optional<_Ty> const& x)
{
	return bool(x);
}

template<typename _Ty>
inline bool operator<(optional<_Ty> const&, nullopt_t)
{
	return false;
}

template<typename _Ty>
inline bool operator<(nullopt_t, optional<_Ty> const& x)
{
	return bool(x);
}

template<typename _Ty>
inline bool operator<=(optional<_Ty> const& x, nullopt_t)
{
	return (!x);
}

template<typename _Ty>
inline bool operator<=(nullopt_t, optional<_Ty> const&)
{
	return true;
}

template<typename _Ty>
inline bool operator>(optional<_Ty> const& x, nullopt_t)
{
	return bool(x);
}

template<typename _Ty>
inline bool operator>(nullopt_t, optional<_Ty> const&)
{
	return false;
}

template<typename _Ty>
inline bool operator>=(optional<_Ty> const&, nullopt_t)
{
	return true;
}

template<typename _Ty>
inline bool operator>=(nullopt_t, optional<_Ty> const& x)
{
	return (!x);
}

// Comparison with T

template<typename _Ty, typename _Uty>
inline bool operator==(optional<_Ty> const& x, _Uty const& v)
{
	return bool(x) ? *x == v : false;
}

template<typename _Ty, typename _Uty>
inline bool operator==(_Uty const& v, optional<_Ty> const& x)
{
	return bool(x) ? v == *x : false;
}

template<typename _Ty, typename _Uty>
inline bool operator!=(optional<_Ty> const& x, _Uty const& v)
{
	return bool(x) ? *x != v : true;
}

template<typename _Ty, typename _Uty>
inline bool operator!=(_Uty const& v, optional<_Ty> const& x)
{
	return bool(x) ? v != *x : true;
}

template<typename _Ty, typename _Uty>
inline bool operator<(optional<_Ty> const& x, _Uty const& v)
{
	return bool(x) ? *x < v : true;
}

template<typename _Ty, typename _Uty>
inline bool operator<(_Uty const& v, optional<_Ty> const& x)
{
	return bool(x) ? v < *x : false;
}

template<typename _Ty, typename _Uty>
inline bool operator<=(optional<_Ty> const& x, _Uty const& v)
{
	return bool(x) ? *x <= v : true;
}

template<typename _Ty, typename _Uty>
inline bool operator<=(_Uty const& v, optional<_Ty> const& x)
{
	return bool(x) ? v <= *x : false;
}

template<typename _Ty, typename _Uty>
inline bool operator>(optional<_Ty> const& x, _Uty const& v)
{
	return bool(x) ? *x > v : false;
}

template<typename _Ty, typename _Uty>
inline bool operator>(_Uty const& v, optional<_Ty> const& x)
{
	return bool(x) ? v > *x : true;
}

template<typename _Ty, typename _Uty>
inline bool operator>=(optional<_Ty> const& x, _Uty const& v)
{
	return bool(x) ? *x >= v : false;
}

template<typename _Ty, typename _Uty>
inline bool operator>=(_Uty const& v, optional<_Ty> const& x)
{
	return bool(x) ? v >= *x : true;
}

template <class _Ty>
void swap(optional<_Ty>& x, optional<_Ty>& y)
{
	x.swap(y);
}

template <class _Ty>
inline optional<_Ty> make_optional(const _Ty& value)
{
	return optional<_Ty>(value);
}

_WHIP_END