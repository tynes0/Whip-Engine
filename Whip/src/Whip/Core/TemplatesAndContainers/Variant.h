#pragma once

#include <Whip/Core/Core.h>
#include <Whip/Core/Log.h>
#include <Whip/Core/TemplatesAndContainers/TypeTraits.h>
#include <Whip/Core/TemplatesAndContainers/Utility.h>
#include <Whip/Core/TemplatesAndContainers/Hash.h>

#include <exception>
#include <type_traits>
#include <utility>
#include <limits>
#include <initializer_list>
#include <memory>

_WHIP_START

#if _HAS_CXX20

#define WHP_FWD(x) static_cast<decltype(x)&&>(x)
#define WHP_MOV(x) static_cast<std::remove_reference_t<decltype(x)>&&>(x)

class bad_variant_access final : std::exception
{
public:
	bad_variant_access(const char* message) noexcept : m_message(message) {}
	bad_variant_access() noexcept = default;
	bad_variant_access(const bad_variant_access&) noexcept = default;
	bad_variant_access& operator=(const bad_variant_access&) noexcept = default;
	const char* what() const noexcept override { return m_message; }

private:
	const char* m_message = "";
};

namespace var_detail
{
	struct variant_tag {};
	struct emplacer_tag {};
}

template <class _Ty>
struct in_place_type_t : private var_detail::emplacer_tag {};

template <size_t _Index>
struct in_place_index_t : private var_detail::variant_tag {};

template <class _Ty>
inline static constexpr in_place_type_t<_Ty> in_place_type;

template <size_t _Index>
inline static constexpr in_place_index_t<_Index> in_place_index;

namespace var_detail
{
	template <int N>
	constexpr int find_first_true(bool(&& arr)[N])
	{
		for (int k = 0; k < N; ++k)
			if (arr[k])
				return k;
		return -1;
	}

	template <class _Ty, class... _Args>
	inline constexpr bool appears_exactly_once = (static_cast<unsigned short>(std::is_same_v<_Ty, _Args>) + ...) == 1;

	template <unsigned char = 1>
	struct find_type_i;

	template <>
	struct find_type_i<1>
	{
		template <size_t _Idx, class _Ty, class... _Args>
		using f = typename find_type_i<(_Idx != 1)>::template f<_Idx - 1, _Args...>;
	};

	template <>
	struct find_type_i<0>
	{
		template <size_t, class _Ty, class... _Args>
		using f = _Ty;
	};

	template <size_t K, class... _Args>
	using type_pack_element = typename find_type_i<(K != 0)>::template f<K, _Args...>;

	template <class _Ty>
	using arr1 = _Ty[1];

	template <size_t N, class _Ty>
	struct overload_frag
	{
		using type = _Ty;
		template <class _Uty>
			requires requires {arr1<_Ty>{std::declval<_Uty>()}; }
		auto operator()(_Ty, _Uty&&)->overload_frag<N, _Ty>;
	};

	template <class _Seq, class... _Args>
	struct make_overload;

	template <size_t... _Idxs, class... _Args>
	struct make_overload<std::integer_sequence<size_t, _Idxs...>, _Args...>
		: overload_frag<_Idxs, _Args>... { using overload_frag<_Idxs, _Args>::operator()...; };

	template <class _Ty, class... _Args>
	using best_overload_match = typename decltype(make_overload<std::make_index_sequence<sizeof...(_Args)>, _Args...>{}(std::declval<_Ty>(), std::declval<_Ty>()))::type;

	template <class _Ty, class... _Args>
	concept has_non_ambiguous_match = requires { typename best_overload_match<_Ty, _Args...>; };

	template <class _From, class _To>
	concept convertible = is_convertible_v<_From, _To>;

	template <class _Ty>
	concept has_eq_comp = requires (_Ty a, _Ty b)
	{
		{ a == b } -> convertible<bool>;
	};

	template <class _Ty>
	concept has_lesser_comp = requires (_Ty a, _Ty b)
	{
		{ a < b } -> convertible<bool>;
	};

	template <class _Ty>
	concept has_less_or_eq_comp = requires (_Ty a, _Ty b)
	{
		{ a <= b } -> convertible<bool>;
	};

	template <class _Uty>
	struct emplace_no_dtor_from_elem
	{
		template <class _Ty>
		constexpr void operator()(_Ty&& elem, auto index_) const
		{
			a.template emplace_no_dtor<index_>(static_cast<_Ty&&>(elem));
		}
		_Uty& a;
	};

	template <class _Uty, class _Ty>
	constexpr void destruct(_Ty& obj)
	{
		if constexpr (!std::is_trivially_destructible_v<_Uty>)
			obj.~_Uty();
	}

	struct dummy_type { static constexpr int elem_size = 0; };
	using union_index_t = unsigned int;

#define TRAIT(trait) (std::is_##trait##_v<A> && std::is_##trait##_v<B>)
#define SFM(signature, trait) signature = default; signature requires (TRAIT(trait) && !TRAIT(trivially_##trait)) {}
#define INJECT_UNION_SFM(X) \
	SFM(constexpr X (const X &), copy_constructible) \
	SFM(constexpr X (X&&), move_constructible) \
	SFM(constexpr X& operator=(const X&), copy_assignable) \
	SFM(constexpr X& operator=(X&&), move_assignable) \
	SFM(constexpr ~X(), destructible)

	template <bool _IsLeaf>
	struct node_trait;

	template <>
	struct node_trait<true>
	{
		template <class A, class B>
		static constexpr auto elem_size = !(std::is_same_v<B, dummy_type>) ? 2 : 1;

		template <size_t, class>
		static constexpr char ctor_branch = 0;
	};

	template <>
	struct node_trait<false>
	{
		template <class A, class B>
		static constexpr auto elem_size = A::elem_size + B::elem_size;

		template <size_t _Index, class A>
		static constexpr char ctor_branch = (_Index < A::elem_size) ? 1 : 2;
	};

	template <bool _IsLeaf, class A, class B>
	struct union_node
	{
		union
		{
			A a;
			B b;
		};

		static constexpr auto elem_size = node_trait<_IsLeaf>::template elem_size<A, B>;

		constexpr union_node() = default;

		template <size_t _Index, class... _Args>
			requires (node_trait<_IsLeaf>::template ctor_branch<_Index, A> == 1)
		constexpr union_node(in_place_index_t<_Index>, _Args&&... args)
			: a(in_place_index<_Index>, static_cast<_Args&&>(args)...) {}

		template <size_t _Index, class... _Args>
			requires (node_trait<_IsLeaf>::template ctor_branch<_Index, A> == 2)
		constexpr union_node(in_place_index_t<_Index>, _Args&&... args)
			: b(in_place_index<_Index - A::elem_size>, static_cast<_Args&&>(args)...) {}

		template <class... _Args>
			requires(_IsLeaf)
		constexpr union_node(in_place_index_t<0>, _Args&&... args)
			: a(static_cast<_Args&&>(args)...) {}

		template <class ... _Args>
			requires(_IsLeaf)
		constexpr union_node(in_place_index_t<1>, _Args&&... args)
			: b(static_cast<_Args&&>(args)...) {}

		constexpr union_node(dummy_type)
			requires (std::is_same_v<dummy_type, B>)
		: b() {}

		template <union_index_t _Index>
		constexpr auto& get()
		{
			if constexpr (_IsLeaf)
			{
				if constexpr (_Index == 0)
					return a;
				else
					return b;
			}
			else
			{
				if constexpr (_Index < A::elem_size)
					return a.template get<_Index>();
				else
					return b.template get<_Index - A::elem_size>();
			}
		}

		INJECT_UNION_SFM(union_node);
	};
#undef INJECT_UNION_SFM
#undef SFM
#undef TRAIT

	constexpr unsigned char pick_next(unsigned int remaining)
	{
		return remaining >= 2 ? 2 : remaining;
	}

	template <unsigned char Pick, unsigned char GoOn, bool FirstPass>
	struct make_tree;

	template <bool IsFirstPass>
	struct make_tree<2, 1, IsFirstPass>
	{
		template <unsigned Remaining, class A, class B, class... Ts>
		using f = typename make_tree<pick_next(Remaining - 2), sizeof...(Ts) != 0, IsFirstPass>::template f<Remaining - 2, Ts..., union_node<IsFirstPass, A, B>>;
	};

	template <bool F>
	struct make_tree<0, 0, F>
	{
		template <unsigned, class A>
		using f = A;
	};

	template <bool IsFirstPass>
	struct make_tree<0, 1, IsFirstPass>
	{
		template <unsigned Remaining, class... Ts>
		using f = typename make_tree<pick_next(sizeof...(Ts)), (sizeof...(Ts) != 1), false>::template f<sizeof...(Ts), Ts...>;
	};

	template <>
	struct make_tree<1, 1, false>
	{
		template <unsigned Remaining, class A, class... Ts>
		using f = typename make_tree<0, sizeof...(Ts) != 0, false>::template f<0, Ts..., A>;
	};

	template <>
	struct make_tree<1, 1, true>
	{
		template <unsigned, class A, class... Ts>
		using f = typename make_tree<0, sizeof...(Ts) != 0, false>::template f<0, Ts..., union_node<true, A, dummy_type>>;
	};

	template <class... Ts>
	using make_tree_union = typename make_tree<pick_next(sizeof...(Ts)), 1, true>::template f<sizeof...(Ts), Ts...>;

	template <size_t Num, class... Ts>
	using smallest_suitable_integer_type = type_pack_element<(static_cast<unsigned char>(Num > std::numeric_limits<Ts>::max()) + ...), Ts...>;

	// why do we need this again? i think something to do with GCC? 
	namespace swap_trait
	{
		template <class A>
		concept able = requires (A a, A b) { swap(a, b); };

		template <class A>
		inline constexpr bool nothrow = noexcept(swap(std::declval<A&>(), std::declval<A&>()));
	}

			template <class T>
		inline constexpr bool has_whip_hash = requires (T t) 
		{
			std::size_t(::whip::hash<std::remove_cvref_t<T>>{}(t));
		};

	template <class Fn, class... Vars>
	using rtype_visit = decltype((std::declval<Fn>()(std::declval<Vars>().template unsafe_get<0>()...)));

	template <class Fn, class Var>
	using rtype_index_visit = decltype((std::declval<Fn>()(std::declval<Var>().template unsafe_get<0>(), std::integral_constant<std::size_t, 0>{})));

	inline namespace extra_detail
	{
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
#define DeclareUnreachable() __builtin_unreachable()
#elif defined (_MSC_VER)
#define DeclareUnreachable() __assume(false)
#else
#error "Compiler not supported, please file an issue."
#endif

#define DEC(N) X((N)) X((N) + 1) X((N) + 2) X((N) + 3) X((N) + 4) X((N) + 5) X((N) + 6) X((N) + 7) X((N) + 8) X((N) + 9)

#define SEQ30(N) DEC( (N) + 0 ) DEC( (N) + 10 ) DEC( (N) + 20 ) 
#define SEQ100(N) SEQ30((N) + 0) SEQ30((N) + 30) SEQ30((N) + 60) DEC((N) + 90) 
#define SEQ200(N) SEQ100((N) + 0) SEQ100((N) + 100)
#define SEQ400(N) SEQ200((N) + 0) SEQ200((N) + 200)
#define CAT(M, N) M##N
#define CAT2(M, N) CAT(M, N)
#define INJECTSEQ(N) CAT2(SEQ, N)(0)

		template <unsigned Offset, class Rtype, class Fn, class V>
		constexpr Rtype single_visit_tail(Fn&& fn, V&& v)
		{
			constexpr auto RemainingIndex = std::decay_t<V>::size - Offset;

#define X(N) case (N + Offset) : \
		if constexpr (N < RemainingIndex) { \
			return static_cast<Fn&&>(fn)( static_cast<V&&>(v).template unsafe_get<N+Offset>() ); \
			break; \
		} else DeclareUnreachable();

#define SEQSIZE 200

			switch (v.index()) 
			{
				INJECTSEQ(SEQSIZE)
			default:
				if constexpr (SEQSIZE < RemainingIndex)
					return var_detail::single_visit_tail<Offset + SEQSIZE, Rtype>(static_cast<Fn&&>(fn), static_cast<V&&>(v));
				else
					DeclareUnreachable();
			}
#undef X
#undef SEQSIZE
		}

		template <unsigned Offset, class Rtype, class Fn, class V>
		constexpr Rtype single_visit_w_index_tail(Fn&& fn, V&& v) {

			constexpr auto RemainingIndex = std::decay_t<V>::size - Offset;

#define X(N) case (N + Offset) : \
		if constexpr (N < RemainingIndex) { \
			return static_cast<Fn&&>(fn)( static_cast<V&&>(v).template unsafe_get<N+Offset>(), std::integral_constant<unsigned, N+Offset>{} ); \
			break; \
		} else DeclareUnreachable();

#define SEQSIZE 200

			switch (v.index())
			{
				INJECTSEQ(SEQSIZE)

			default:
				if constexpr (SEQSIZE < RemainingIndex)
					return var_detail::single_visit_w_index_tail<Offset + SEQSIZE, Rtype>(static_cast<Fn&&>(fn), static_cast<V&&>(v));
				else
					DeclareUnreachable();
			}

#undef X
#undef SEQSIZE
		}

		template <class Fn, class V>
		constexpr decltype(auto) visit(Fn&& fn, V&& v)
		{
			return var_detail::single_visit_tail<0, rtype_visit<Fn&&, V&&>>(WHP_FWD(fn), WHP_FWD(v));
		}

		template <class Fn, class V>
		constexpr decltype(auto) visit_with_index(V&& v, Fn&& fn) 
		{
			return var_detail::single_visit_w_index_tail<0, rtype_index_visit<Fn&&, V&&>>(WHP_FWD(fn), WHP_FWD(v));
		}

		template <class Fn, class Head, class... Tail>
		constexpr decltype(auto) multi_visit(Fn&& fn, Head&& head, Tail&&... tail) 
		{

			// visit them one by one, starting with the last
			auto vis = [&fn, &head](auto&&... args) -> decltype(auto) 
			{
				return var_detail::visit([&fn, &args...](auto&& elem) -> decltype(auto) 
					{
					return WHP_FWD(fn)(WHP_FWD(elem), WHP_FWD(args)...);
					}, WHP_FWD(head));
			};

			if constexpr (sizeof...(tail) == 0)
				return WHP_FWD(vis)();
			else if constexpr (sizeof...(tail) == 1)
				return var_detail::visit(WHP_FWD(vis), WHP_FWD(tail)...);
			else
				return var_detail::multi_visit(WHP_FWD(vis), WHP_FWD(tail)...);
		}

#undef DEC
#undef SEQ30
#undef SEQ100
#undef SEQ200
#undef SEQ400
#undef DeclareUnreachable
#undef CAT
#undef CAT2
#undef INJECTSEQ
	}

	struct variant_npos_t {
		template <class T>
		constexpr bool operator==(T idx) const noexcept { return idx == std::numeric_limits<T>::max(); }
	};
}

template <class T>
inline constexpr bool is_variant = std::is_base_of_v<var_detail::variant_tag, std::decay_t<T>>;

inline static constexpr var_detail::variant_npos_t variant_npos;

template <class... Ts>
class variant;

template <class... Ts>
	requires((std::is_array_v<Ts> || ...) || (std::is_reference_v<Ts> || ...) || (std::is_void_v<Ts> || ...) || sizeof...(Ts) == 0)
class variant<Ts...>
{
	static_assert(sizeof...(Ts) > 0, "A variant cannot be empty.");
	static_assert(!(std::is_reference_v<Ts> || ...), "A variant cannot contain references, consider using reference wrappers instead.");
	static_assert(!(std::is_void_v<Ts> || ...), "A variant cannot contains void.");
	static_assert(!(std::is_array_v<Ts> || ...), "A variant cannot contain a raw array type, consider using whip::array instead.");
};

template <class... Ts>
class variant : private var_detail::variant_tag
{
private:
	using storage_t = var_detail::union_node<false, var_detail::make_tree_union<Ts...>, var_detail::dummy_type>;

	static constexpr bool is_trivial = std::is_trivial_v<storage_t>;
	static constexpr bool has_copy_ctor = std::is_copy_constructible_v<storage_t>;
	static constexpr bool trivial_copy_ctor = is_trivial || std::is_trivially_copy_constructible_v<storage_t>;
	static constexpr bool has_copy_assign = std::is_copy_constructible_v<storage_t>;
	static constexpr bool trivial_copy_assign = is_trivial || std::is_trivially_copy_assignable_v<storage_t>;
	static constexpr bool has_move_ctor = std::is_move_constructible_v<storage_t>;
	static constexpr bool trivial_move_ctor = is_trivial || std::is_trivially_move_constructible_v<storage_t>;
	static constexpr bool has_move_assign = std::is_move_assignable_v<storage_t>;
	static constexpr bool trivial_move_assign = is_trivial || std::is_trivially_move_assignable_v<storage_t>;
	static constexpr bool trivial_dtor = std::is_trivially_destructible_v<storage_t>;
public:

	template <size_t Idx>
	using alternative = std::remove_reference_t<decltype(std::declval<storage_t&>().template get<Idx>())>;

	static constexpr bool can_be_valueless = !is_trivial;

	static constexpr unsigned size = sizeof...(Ts);

	using index_type = var_detail::smallest_suitable_integer_type<sizeof...(Ts) + can_be_valueless, unsigned char, unsigned short, unsigned>;

	static constexpr index_type npos = -1;

	template <class _Ty>
	static constexpr int index_of = var_detail::find_first_true({ std::is_same_v<_Ty, Ts>... });

	constexpr variant()
		noexcept(std::is_nothrow_default_constructible_v<alternative<0>>)
		requires std::is_default_constructible_v<alternative<0>>
	: storage(in_place_index<0>), current(0) {}

	// copy constructor trivial 
	constexpr variant(const variant&) requires trivial_copy_ctor = default;

	// copy constructor default
	constexpr variant(const variant& other) requires (has_copy_ctor && !trivial_copy_ctor)
		: storage(var_detail::dummy_type{})
	{
		construct_from(other);
	}

	// move constructor
	constexpr variant(variant&& other) noexcept ((std::is_nothrow_move_constructible_v<Ts> && ...))
		requires (has_move_ctor && !trivial_move_ctor)
	: storage(var_detail::dummy_type{})
	{
		construct_from(static_cast<variant&&>(other));
	}

	// generic constructor
	template <class _Ty, class _M = var_detail::best_overload_match<_Ty&&, Ts...>, class _D = std::decay_t<_Ty>>
	constexpr variant(_Ty&& data) noexcept (std::is_nothrow_constructible_v<_M, _Ty&&>)
		requires(!std::is_same_v<_D, variant> && !std::is_base_of_v<var_detail::emplacer_tag, _D>)
	: variant(in_place_index<index_of<_M>>, static_cast<_Ty&&>(data)) {}

	template <size_t _Index, class... _Args>
		requires (_Index < size&& std::is_constructible_v<alternative<_Index>, _Args&&...>)
	explicit constexpr variant(in_place_index_t<_Index> tag, _Args&&... args)
		: storage(tag, static_cast<_Args&&>(args)...), current(_Index) { }

	template <class T, class... Args>
		requires (var_detail::appears_exactly_once<T, Ts...>&& std::is_constructible_v<T, Args&&...>)
	explicit constexpr variant(in_place_type_t<T>, Args&&... args)
		: variant(in_place_index<index_of<T>>, static_cast<Args&&>(args)...) {}

	template <std::size_t Index, class U, class... Args>
		requires ((Index < size) && std::is_constructible_v<alternative<Index>, std::initializer_list<U>&, Args&&...>)
	explicit constexpr variant(in_place_index_t<Index> tag, std::initializer_list<U> list, Args&&... args)
		: storage(tag, list, WHP_FWD(args)...), current(Index) {}

	template <class _Ty, class _Uty, class... _Args>
		requires(var_detail::appears_exactly_once<_Ty, Ts...>&& std::is_constructible_v<_Ty, std::initializer_list<_Uty>&, _Args&&...>)
	explicit constexpr variant(in_place_type_t<_Ty>, std::initializer_list<_Uty> ilist, _Args&&... args)
		: storage(in_place_index<index_of<_Ty>>, ilist, WHP_FWD(args)...), current(index_of<_Ty>) {}

	constexpr ~variant() requires trivial_dtor = default;

	constexpr ~variant() requires (!trivial_dtor)
	{
		reset();
	}

	constexpr variant& operator=(const variant& rhs)
		requires (has_copy_assign && !(trivial_copy_assign && trivial_copy_ctor))
	{
		assign_from(rhs, [this](const auto& elem, auto index_cst)
			{
				if (index() == index_cst)
					unsafe_get<index_cst>() = elem;
				else
				{
					using type = alternative<index_cst>;
					if constexpr (std::is_nothrow_copy_constructible_v<type> || !std::is_nothrow_move_constructible_v<type>)
						this->emplace<index_cst>(elem);
					else
					{
						alternative<index_cst> tmp = elem;
						this->emplace<index_cst>(WHP_MOV(tmp));
					}
				}
			});
		return *this;
	}

	constexpr variant& operator=(variant&& other) requires (trivial_move_assign && trivial_move_ctor && trivial_dtor) = default;

	// move assignment
	constexpr variant& operator=(variant&& other)
		noexcept ((std::is_nothrow_move_constructible_v<Ts> && ...) && (std::is_nothrow_move_assignable_v<Ts> && ...))
		requires (has_move_assign && has_move_ctor && !(trivial_move_assign && trivial_move_ctor && trivial_dtor))
	{
		this->assign_from(WHP_FWD(other), [this](auto&& elem, auto index_cst)
			{
				if (index() == index_cst)
					this->unsafe_get<index_cst>() = WHP_MOV(elem);
				else
					this->emplace<index_cst>(WHP_MOV(elem));
			});
		return *this;
	}

	template <class T>
		requires var_detail::has_non_ambiguous_match<T, Ts...>
	constexpr variant& operator=(T&& t)
		noexcept(std::is_nothrow_assignable_v<var_detail::best_overload_match<T&&, Ts...>, T&&>
			&& std::is_nothrow_constructible_v<var_detail::best_overload_match<T&&, Ts...>, T&&>)
	{
		using related_type = var_detail::best_overload_match<T&&, Ts...>;
		constexpr auto new_index = index_of<related_type>;

		if (this->current == new_index)
			this->unsafe_get<new_index>() = WHP_FWD(t);
		else
		{

			constexpr bool do_simple_emplace = std::is_nothrow_constructible_v<related_type, T> || !std::is_nothrow_move_constructible_v<related_type>;

			if constexpr (do_simple_emplace)
				this->emplace<new_index>(WHP_FWD(t));
			else
			{
				related_type tmp = t;
				this->emplace<new_index>(WHP_MOV(tmp));
			}
		}
		return *this;
	}

	template <class T, class... Args>
		requires (std::is_constructible_v<T, Args&&...>&& var_detail::appears_exactly_once<T, Ts...>)
	constexpr T& emplace(Args&&... args)
	{
		return emplace<index_of<T>>(static_cast<Args&&>(args)...);
	}

	template <size_t Idx, class... Args>
		requires (Idx < size and std::is_constructible_v<alternative<Idx>, Args&&...>)
	constexpr auto& emplace(Args&&... args)
	{
		return emplace_impl<Idx>(WHP_FWD(args)...);
	}

	template <size_t Idx, class U, class... Args>
		requires (Idx < size&& std::is_constructible_v<alternative<Idx>, std::initializer_list<U>&, Args&&...>)
	constexpr auto& emplace(std::initializer_list<U> list, Args&&... args) {
		return emplace_impl<Idx>(list, WHP_FWD(args)...);
	}

	template <class T, class U, class... Args>
		requires (std::is_constructible_v<T, std::initializer_list<U>&, Args&&...>&& var_detail::appears_exactly_once<T, Ts...>)
	constexpr T& emplace(std::initializer_list<U> list, Args&&... args) 
	{
		return emplace_impl<index_of<T>>(list, WHP_FWD(args)...);
	}

	constexpr bool valueless_by_exception() const noexcept
	{
		if constexpr (can_be_valueless)
			return current == npos;
		else
			return false;
	}

	constexpr index_type index() const noexcept
	{
		return current;
	}

	constexpr void swap(variant& o)
		noexcept ((std::is_nothrow_move_constructible_v<Ts> && ...) && (var_detail::swap_trait::template nothrow<Ts> && ...))
		requires (has_move_ctor && (var_detail::swap_trait::template able<Ts> && ...))
	{

		if constexpr (can_be_valueless)
		{
			constexpr auto impl_one_valueless = [](auto&& full, auto& empty)
			{
				var_detail::visit_with_index(WHP_FWD(full), var_detail::emplace_no_dtor_from_elem<variant&>{empty});
				full.reset_no_check();
				full.current = npos;
			};

			switch (static_cast<int>(this->index() == npos) + static_cast<int>(o.index() == npos) * 2)
			{
			case 0:
				break;
			case 1:
				// "this" is valueless
				impl_one_valueless(WHP_MOV(o), *this);
				return;
			case 2:
				// "other" is valueless
				impl_one_valueless(WHP_MOV(*this), o);
				return;
			case 3:
				// both are valueless, do nothing
				return;
			}
		}

		WHP_CORE_ASSERT(!(valueless_by_exception() && o.valueless_by_exception()), "");

		var_detail::visit_with_index(o, [&o, this](auto&& elem, auto index_) {

			if (this->index() == index_) {
				using std::swap;
				swap(this->unsafe_get<index_>(), elem);
				return;
			}

			using idx_t = decltype(index_);
			var_detail::visit_with_index(*this, [this, &o, &elem](auto&& this_elem, auto this_index) {

				auto tmp{ WHP_MOV(this_elem) };

				// destruct the element
				var_detail::destruct<alternative<this_index>>(this_elem);

				// ok, we just destroyed the element in this, don't call the dtor again
				this->emplace_no_dtor<idx_t::value>(WHP_MOV(elem));

				// we could refactor this
				var_detail::destruct<alternative<idx_t::value>>(elem);
				o.template emplace_no_dtor< (unsigned)(this_index) >(WHP_MOV(tmp));

				});
			});
	}

	template <class _Ty>
	constexpr auto& unsafe_get() & noexcept
	{
		constexpr var_detail::union_index_t Idx = (index_of<_Ty>);
		static_assert(Idx < size, "Requested type is not contained in the whip::variant");
		WHP_CORE_ASSERT(current == Idx, "Requested type is not the current type in the whip::variant");
		return storage.template get<Idx>();
	}

	template <var_detail::union_index_t Idx>
	constexpr auto& unsafe_get() & noexcept
	{
		static_assert(Idx < size, "Requested type is not contained in the whip::variant");
		WHP_CORE_ASSERT(current == Idx, "Requested type is not the current type in the whip::variant");
		return storage.template get<Idx>();
	}

	template <var_detail::union_index_t Idx>
	constexpr auto&& unsafe_get() && noexcept
	{
		static_assert(Idx < size, "Requested type is not contained in the whip::variant");
		WHP_CORE_ASSERT(current == Idx, "Requested type is not the current type in the whip::variant");
		return WHP_MOV(storage.template get<Idx>());
	}

	template <var_detail::union_index_t Idx>
	constexpr const auto& unsafe_get() const& noexcept
	{
		static_assert(Idx < size, "Requested type is not contained in the whip::variant");
		WHP_CORE_ASSERT(current == Idx, "Requested type is not the current type in the whip::variant");
		return const_cast<variant&>(*this).unsafe_get<Idx>();
	}

	template <var_detail::union_index_t Idx>
	constexpr const auto&& unsafe_get() const&& noexcept
	{
		static_assert(Idx < size, "Requested type is not contained in the whip::variant");
		WHP_CORE_ASSERT(current == Idx, "Requested type is not the current type in the whip::variant");
		return WHP_MOV(unsafe_get<Idx>());
	}

private:
	// assign from another variant
	template <class Other, class Fn>
	constexpr void assign_from(Other&& o, Fn&& fn) 
	{
		if constexpr (can_be_valueless) 
		{
			if (o.index() == npos) 
			{
				if (current != npos) 
				{
					reset_no_check();
					current = npos;
				}
				return;
			}
		}
		WHP_CORE_ASSERT(!o.valueless_by_exception(), "whip::variant : valueless by exception");
		var_detail::visit_with_index(WHP_FWD(o), WHP_FWD(fn));
	}

	template <unsigned Idx, class... Args>
	constexpr auto& emplace_impl(Args&&... args)
	{
		reset();
		emplace_no_dtor<Idx>(WHP_FWD(args)...);
		return unsafe_get<Idx>();
	}

	template <unsigned Idx, class... Args>
	constexpr void emplace_no_dtor(Args&&... args)
	{
		using T = alternative<Idx>;

		if constexpr (!std::is_nothrow_constructible_v<T, Args&&...>)
		{
			if constexpr (std::is_nothrow_move_constructible_v<T>)
				do_emplace_no_dtor<Idx>(T{ WHP_FWD(args)... });
			else if constexpr (std::is_nothrow_copy_constructible_v<T>)
			{
				T tmp{ WHP_FWD(args)... };
				do_emplace_no_dtor<Idx>(tmp);
			}
			else
			{
				static_assert(can_be_valueless && (Idx == Idx), "Internal error : the possibly valueless branch of emplace was taken despite |can_be_valueless| being false");
				current = npos;
				do_emplace_no_dtor<Idx>(WHP_FWD(args)...);
			}
		}
		else
			do_emplace_no_dtor<Idx>(WHP_FWD(args)...);
	}

	template <unsigned Idx, class... Args>
	constexpr void do_emplace_no_dtor(Args&&... args)
	{
		auto* ptr = whip::addressof(unsafe_get<Idx>());
		std::construct_at(ptr, WHP_FWD(args)...);
		current = static_cast<index_type>(Idx);
	}

	constexpr void reset() 
	{
		if constexpr (can_be_valueless)
			if (valueless_by_exception()) return;
		reset_no_check();
	}

	constexpr void reset_no_check() 
	{
		WHP_CORE_ASSERT(index() < size, "Requested type is not the current type in the whip::variant");
		if constexpr (!trivial_dtor) 
		{
			var_detail::visit_with_index(*this, [](auto& elem, auto index_) 
				{
				var_detail::destruct<alternative<index_>>(elem);
				});
		}
	}

	// construct this from another variant, for constructors only
	template <class Other>
	constexpr void construct_from(Other&& o) 
	{
		if constexpr (can_be_valueless)
			if (o.valueless_by_exception()) 
			{
				current = npos;
				return;
			}

		var_detail::visit_with_index(WHP_FWD(o), var_detail::emplace_no_dtor_from_elem<variant&>{*this});
	}

	template <class T>
	friend struct var_detail::emplace_no_dtor_from_elem;

	storage_t storage;
	index_type current;
};

template <class T, class... Ts>
constexpr bool holds_alternative(const variant<Ts...>& v) noexcept
{
	static_assert((std::is_same_v<T, Ts> || ...), "Requested type is not contained in the variant");
	constexpr auto Index = variant<Ts...>::template index_of<T>;
	return v.index() == Index;
}

template <std::size_t Idx, class... Ts>
constexpr auto& get(variant<Ts...>& v) {
	static_assert(Idx < sizeof...(Ts), "Index exceeds the variant size. ");
	if (v.index() != Idx) throw bad_variant_access{ "whip::variant : Bad variant access in get." };
	return (v.template unsafe_get<Idx>());
}

template <std::size_t Idx, class... Ts>
constexpr const auto& get(const variant<Ts...>& v)
{
	return whip::get<Idx>(const_cast<variant<Ts...>&>(v));
}

template <std::size_t Idx, class... Ts>
constexpr auto&& get(variant<Ts...>&& v)
{
	return WHP_MOV(whip::get<Idx>(v));
}

template <std::size_t Idx, class... Ts>
constexpr const auto&& get(const variant<Ts...>&& v)
{
	//whip::
	return WHP_MOV(whip::get<Idx>(v));
}

template <class T, class... Ts>
constexpr T& get(variant<Ts...>& v)
{
	return whip::get<variant<Ts...>::template index_of<T>>(v);
}

template <class T, class... Ts>
constexpr const T& get(const variant<Ts...>& v)
{
	return whip::get<variant<Ts...>::template index_of<T>>(v);
}

template <class T, class... Ts>
constexpr T&& get(variant<Ts...>&& v)
{
	return whip::get<variant<Ts...>::template index_of<T>>(WHP_FWD(v));
}

template <class T, class... Ts>
constexpr const T&& get(const variant<Ts...>&& v)
{
	return whip::get<variant<Ts...>::template index_of<T>>(WHP_FWD(v));
}

template <std::size_t Idx, class... Ts>
constexpr const auto* get_if(const variant<Ts...>* v) noexcept
{
	using rtype = typename variant<Ts...>::template alternative<Idx>*;
	if (v == nullptr || v->index() != Idx)
		return rtype{ nullptr };
	else
		return whip::addressof(v->template unsafe_get<Idx>());
}

template <std::size_t Idx, class... Ts>
constexpr auto* get_if(variant<Ts...>* v) noexcept
{
	using rtype = typename variant<Ts...>::template alternative<Idx>;
	return const_cast<rtype*>(get_if<Idx>(static_cast<const variant<Ts...>*>(v)));
}

template <class Fn, class... Vs>
constexpr decltype(auto) visit(Fn&& fn, Vs&&... vs)
{
	if constexpr ((std::decay_t<Vs>::can_be_valueless || ...))
		if ((vs.valueless_by_exception() || ...))
			throw bad_variant_access{ "whip::variant : Bad variant access in visit." };

	if constexpr (sizeof...(Vs) == 1)
		return var_detail::visit(WHP_FWD(fn), WHP_FWD(vs)...);
	else
		return var_detail::multi_visit(WHP_FWD(fn), WHP_FWD(vs)...);
}

template <class Fn>
constexpr decltype(auto) visit(Fn&& fn)
{
	return WHP_FWD(fn)();
}

template <class R, class Fn, class... Vs>
	requires (is_variant<Vs> && ...)
constexpr R visit(Fn&& fn, Vs&&... vars)
{
	return static_cast<R>(visit(WHP_FWD(fn), WHP_FWD(vars)...));
}

template <class... Ts>
	requires (var_detail::has_eq_comp<Ts> && ...)
constexpr bool operator==(const variant<Ts...>& v1, const variant<Ts...>& v2)
{
	if (v1.index() != v2.index())
		return false;
	if constexpr (variant<Ts...>::can_be_valueless)
		if (v1.valueless_by_exception()) return true;
	return var_detail::visit_with_index(v2, [&v1](auto& elem, auto index) -> bool
		{
			return (v1.template unsafe_get<index>() == elem);
		});
}

template <class... Ts>
constexpr bool operator!=(const variant<Ts...>& v1, const variant<Ts...>& v2)
	requires requires { v1 == v2; }
{
	return !(v1 == v2);
}

template <class... Ts>
	requires (var_detail::has_lesser_comp<const Ts&> && ...)
constexpr bool operator<(const variant<Ts...>& v1, const variant<Ts...>& v2)
{
	if constexpr (variant<Ts...>::can_be_valueless)
	{
		if (v2.valueless_by_exception()) return false;
		if (v1.valueless_by_exception()) return true;
	}
	if (v1.index() == v2.index())
	{
		return var_detail::visit_with_index(v1, [&v2](auto& elem, auto index) -> bool
			{
				return (elem < v2.template unsafe_get<index>());
			});
	}
	else
		return (v1.index() < v2.index());
}

template <class... Ts>
constexpr bool operator>(const variant<Ts...>& v1, const variant<Ts...>& v2)
	requires requires { v2 < v1; }
{
	return v2 < v1;
}

template <class... Ts>
	requires (var_detail::has_less_or_eq_comp<const Ts&> && ...)
constexpr bool operator<=(const variant<Ts...>& v1, const variant<Ts...>& v2)
{
	if constexpr (variant<Ts...>::can_be_valueless)
	{
		if (v1.valueless_by_exception()) return true;
		if (v2.valueless_by_exception()) return false;
	}
	if (v1.index() == v2.index())
	{
		return var_detail::visit_with_index(v1, [&v2](auto& elem, auto index) -> bool
			{
				return (elem <= v2.template unsafe_get<index>());
			});
	}
	else
		return (v1.index() < v2.index());
}

template <class... Ts>
constexpr bool operator>=(const variant<Ts...>& v1, const variant<Ts...>& v2)
	requires requires { v2 <= v1; }
{
	return v2 <= v1;
}

struct monostate {};
constexpr bool operator==(monostate, monostate) noexcept { return true; }
constexpr bool operator> (monostate, monostate) noexcept { return false; }
constexpr bool operator< (monostate, monostate) noexcept { return false; }
constexpr bool operator<=(monostate, monostate) noexcept { return true; }
constexpr bool operator>=(monostate, monostate) noexcept { return true; }

template <class... Ts>
constexpr void swap(variant<Ts...>& a, variant<Ts...>& b)
noexcept (noexcept(a.swap(b)))
	requires requires { a.swap(b); }
{
	a.swap(b);
}

template <class T>
	requires is_variant<T>
inline constexpr size_t variant_size_v = std::decay_t<T>::size;

template <class T>
	requires is_variant<T>
struct variant_size : integral_constant<std::size_t, variant_size_v<T>> {};

namespace var_detail
{
	template <bool IsVolatile>
	struct var_alt_impl
	{
		template <size_t Idx, class T>
		using type = remove_reference_t<decltype(std::declval<T>().template unsafe_get<Idx>())>;
	};

	template <>
	struct var_alt_impl<true>
	{
		template <size_t Idx, class T>
		using type = volatile typename var_alt_impl<false>::template type<Idx, std::remove_volatile_t<T>>;
	};
}

template <size_t Idx, class T>
	requires (Idx < variant_size_v<T>)
using variant_alternative_t = typename var_detail::var_alt_impl< std::is_volatile_v<T> >::template type<Idx, T>;

template <size_t Idx, class T>
	requires is_variant<T>
struct variant_alternative
{
	using type = variant_alternative_t<Idx, T>;
};

template <size_t Idx, class Var>
	requires is_variant<Var>
constexpr auto&& unsafe_get(Var&& var) noexcept
{
	static_assert(Idx < std::decay_t<Var>::size, "Index exceeds the variant size.");
	return WHP_FWD(var).template unsafe_get<Idx>();
}

template <class T, class Var>
	requires is_variant<Var>
constexpr auto&& unsafe_get(Var&& var) noexcept
{
	return whip::unsafe_get<std::decay_t<Var>::template index_of<T> >(WHP_FWD(var));
}


template <class... Ts>
		requires (whip::var_detail::has_whip_hash<Ts> && ...)
	struct hash<whip::variant<Ts...>> 
	{
		size_t operator()(const whip::variant<Ts...>& v) const 
		{
			if constexpr (whip::variant<Ts...>::can_be_valueless)
				if (v.valueless_by_exception()) return -1;

			return whip::var_detail::visit_with_index(v, [](auto& elem, auto index_) 
				{
				using type = std::remove_cvref_t<decltype(elem)>;
				return hash<type>{}(elem) + index_;
				});
		}
	};

	template <>
	struct hash<whip::monostate> 
	{
		constexpr size_t operator()(whip::monostate) const noexcept { return -1; }
	};

#undef WHP_FWD
#undef WHP_MOV

#endif // _HAS_CXX20

_WHIP_END