#pragma once

#include "Whip/Core/Core.h"
#include "TypeTraits.h"

_WHIP_START

template <class _Ty, class _Ptr = _Ty*, class _Ref = _Ty&, class _SizeT = size_t, class _Diff = ptrdiff_t>
struct iterator_base
{
	using value_type	= _Ty;
	using pointer		= _Ptr;
	using reference		= _Ref;
	using size_type		= _SizeT;
	using diff_type		= _Diff;
};

template <class _WhipIter>
WHP_INLINE constexpr bool is_const_whip_iterator_v = is_base_of_v<iterator_base<const remove_const_t<typename _WhipIter::value_type>>, _WhipIter>;

template <class _WhipIter>
WHP_INLINE constexpr bool is_non_const_whip_iterator_v = is_base_of_v<iterator_base<remove_const_t<typename _WhipIter::value_type>>, _WhipIter>;

template <class _WhipIter>
WHP_INLINE constexpr bool is_whip_iterator_v = is_const_whip_iterator_v<_WhipIter> || is_non_const_whip_iterator_v<_WhipIter>;

template <class _Iter, enable_if_t<is_whip_iterator_v<_Iter>, int> = 0>
class reverse_iterator : public iterator_base<typename _Iter::value_type>
{
public:
	using m_base		= _Iter;
	using value_type	= typename m_base::value_type;
	using pointer		= typename m_base::pointer;
	using reference		= typename m_base::reference;
	using size_type		= typename m_base::size_type;
	using diff_type		= typename m_base::diff_type;

	reverse_iterator() = default;

	reverse_iterator(const _Iter& right) : current_iterator(move(right)) {}

	reverse_iterator(const reverse_iterator& right) : current_iterator(right.current_iterator) {}

	reverse_iterator& operator=(const reverse_iterator& right)
	{
		current_iterator = right.current_iterator;
		return *this;
	}

	WHP_NODISCARD _Iter base() const
	{
		return current_iterator;
	}

	WHP_NODISCARD reference operator*() const 
	{
		_Iter temp = current_iterator;
		return *(--temp);
	}

	// !!!!!!!!! DANGER !!!!!!!!! todo: we should improve
	WHP_NODISCARD pointer operator->() const
	{
		_Iter temp = current_iterator;
		--temp;
		return temp.operator->();
	}

	reverse_iterator& operator++()
	{
		--current_iterator;
		return *this;
	}

	reverse_iterator operator++(int)
	{
		_Iter temp = current_iterator;
		current_iterator--;
		return temp;
	}

	reverse_iterator& operator--()
	{
		++current_iterator;
		return *this;
	}

	reverse_iterator operator--(int)
	{
		_Iter temp = current_iterator;
		current_iterator++;
		return temp;
	}

	reverse_iterator operator+(diff_type n) const
	{
		return reverse_iterator(current_iterator - n);
	}

	reverse_iterator operator-(diff_type n) const
	{
		return reverse_iterator(current_iterator + n);
	}

	bool operator==(const reverse_iterator& other)
	{
		return (this->current_iterator == other.current_iterator);
	}

	bool operator==(reverse_iterator&& other)
	{
		return (this->current_iterator == other.current_iterator);
	}

	bool operator!=(const reverse_iterator& other)
	{
		return !(this->operator==(move(other)));
	}

private:
	_Iter current_iterator{};
};

_WHIP_END