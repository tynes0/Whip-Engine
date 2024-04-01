#pragma once
#ifndef _WHIP_STRING_OPERATIONS_
#define _WHIP_STRING_OPERATIONS_

#include <string>

#include "Whip/Core/Core.h"
#include "Whip/Core/Log.h"
#include "Algorithms.h"
#include "Iterator.h"
#include "Vector.h"
#include "OperatorWrapper.h"

#pragma warning(push)
#pragma warning(disable : _WHP_DISABLED_WARNINGS)

_WHIP_START

// --------------------- STRING OPERATIONS ---------------------
namespace string_operations
{
	// check if string starts with given character
	WHP_NODISCARD bool starts_with(const std::string& str, char token);
	// Check if string starts with given string
	WHP_NODISCARD bool starts_with(const std::string& str, const std::string& token);
	// check if string ends with given character
	WHP_NODISCARD bool ends_with(const std::string& str, char token);
	// Check if string ends with given string
	WHP_NODISCARD bool ends_with(const std::string& str, const std::string& token);
	// remove leading spaces and return the removed version
	WHP_NODISCARD std::string trim_left(const std::string& str);
	// remove leading spaces from the reference and return the size of string
	size_t trim_left_directly(std::string& str);
	// remove trailing spaces and return the removed version
	WHP_NODISCARD std::string trim_right(const std::string& str);
	// remove trailing spaces from the reference and return the size of string
	size_t trim_right_directly(std::string& str);
	// remove leading and trailing spaces then return the removed version
	WHP_NODISCARD std::string trim(const std::string& str);
	// remove leading and trailing spaces from the reference then return the size of string
	size_t trim_directly(std::string& str);
	// splits the string in half. the left side returns. the reference is the right side
	std::string chop(std::string& str, size_t offset);
	// splits the string in half. the right side returns. the reference is the left side
	std::string chop_but_left(std::string& str, size_t offset);
	// returns a reversed copy of the string
	WHP_NODISCARD std::string reverse(const std::string& str);
	// reverses a string and returns reference of that
	std::string& reverse_directly(std::string& str);
	// returns a specific field's reverse's in the string
	WHP_NODISCARD std::string reverse_in_place(const std::string& str, size_t offset, size_t length);
	// reverses a specific field in the string
	std::string& reverse_in_place_directly(std::string& str, size_t offset, size_t length);
	// returns the removed copy of the fragment equal to the entered length from the beginning of the string
	WHP_NODISCARD std::string remove_prefix(const std::string& str, size_t length);
	// removes a piece from the beginning of the string up to the entered length and returns reference of that
	std::string& remove_prefix_directly(std::string& str, size_t length);
	// returns the removed copy of the fragment equal to the entered length from the end of the string
	WHP_NODISCARD std::string remove_suffix(const std::string& str, size_t length);
	// removes a piece from the end of the string up to the entered length and returns reference of that
	std::string& remove_suffix_directly(std::string& str, size_t length);
	// returns a copy of the string with all characters in lowercase
	WHP_NODISCARD std::string to_lower(const std::string& str);
	// makes all characters lowercase and returns reference of that
	std::string& to_lower_directly(std::string& str);
	// returns a copy of the string in which the characters in a given field are lowercase.
	WHP_NODISCARD std::string to_lower_in_place(const std::string& str, size_t offset, size_t length);
	// makes characters in a given field lowercase and returns reference of that
	std::string& to_lower_in_place_directly(std::string& str, size_t offset, size_t length);
	// returns a copy of the string with all characters in uppercase
	WHP_NODISCARD std::string to_upper(const std::string& str);
	// makes all characters uppercase and returns reference of that
	std::string& to_upper_directly(std::string& str);
	// returns a copy of the string in which the characters in a given field are uppercase.
	WHP_NODISCARD std::string to_upper_in_place(const std::string& str, size_t offset, size_t length);
	// makes characters in a given field uppercase and returns reference of that
	std::string& to_upper_in_place_directly(std::string& str, size_t offset, size_t length);
	// returns a copy of the string filled with the entered characther
	WHP_NODISCARD std::string filled_copy(const std::string& str, char ch);
	// fills the string with the entered characther
	std::string& fill(std::string& str, char ch);
	// returns a copy of the string filled with the entered character within the entered range
	WHP_NODISCARD std::string fill_in_place_copy(const std::string& str, char ch, size_t offset, size_t length);
	// fills the entered range of the string with the entered character
	std::string& fill_in_place(std::string& str, char ch, size_t offset, size_t length);
	// returns true if all characters in the string are alphanumeric
	WHP_NODISCARD bool is_alnum(const std::string& str);
	// returns true if all characters in the string are in the alphabet
	WHP_NODISCARD bool is_alpha(const std::string& str);
	// returns true if all characters in the string are digits
	WHP_NODISCARD bool is_digit(const std::string& str);
	// returns true if all characters in the string are whitespaces
	WHP_NODISCARD bool is_space(const std::string& str);
	// returns true if all characters in the string are lowercase
	WHP_NODISCARD bool is_lower(const std::string& str);
	// returns true if all characters in the string are uppercase
	WHP_NODISCARD bool is_upper(const std::string& str);
	// returns true if all characters in the string are printable
	WHP_NODISCARD bool is_printable(const std::string& str);
	// separates the string into parts with the entered token
	WHP_NODISCARD vector<std::string> separate_string(const std::string& source, const std::string& token);
	// separates the string into parts with the entered token
	WHP_NODISCARD vector<std::string> separate_string(const std::string& source, char token);

#if !defined(_whip_is_in_defined_) && !defined(is_in)

	namespace detail_string_operations
	{
		whip_operator(in_op, bool, char, const std::string&)
		{
			if (right.find(left) != std::string::npos)
				return true;
			return false;
		}
	}
#define is_in <_WHIP string_operations::detail_string_operations::in_op>
#define _whip_is_in_defined_ 1
#endif // !defined(_whip_is_in_defined_) && !defined(is_in)

	class formatter
	{
		// this is a template class, implementation is only in this file
	public:
		template <typename... Args>
		WHP_NODISCARD static std::string format(const std::string& fmt, Args&&... args)
		{
			if (sizeof...(args) == 0)
			{
				return fmt;
			}

			argument_array argArray;
			transfer_to_array(argArray, args...);
			size_t start = 0;
			size_t pos = 0;
			std::ostringstream oss;
			while (true)
			{
				pos = fmt.find('{', start);
				if (pos == std::string::npos)
				{
					oss << fmt.substr(start);
					break;
				}

				oss << fmt.substr(start, pos - start);
				if (fmt[pos + 1] == '{')
				{
					oss << '{';
					start = pos + 2;
					continue;
				}

				start = pos + 1;
				pos = fmt.find('}', start);
				if (pos == std::string::npos)
				{
					oss << fmt.substr(start - 1);
					break;
				}

				format_item(oss, fmt.substr(start, pos - start), argArray);
				start = pos + 1;
			}

			return oss.str();
		}

		template <typename... Args>
		std::string operator()(const std::string& fmt, Args&&... args)
		{
			return format(fmt, args...);
		}

	private:
		struct argument_base
		{
			argument_base() {}
			virtual ~argument_base() {}
			virtual void format(std::ostringstream&, const std::string&) {}
		};

		template <class _Ty>
		class argument : public argument_base
		{
		public:
			argument(_Ty arg) : m_argument(arg) {}

			virtual ~argument() override {}

			virtual void format(std::ostringstream& oss, const std::string& fmt)
			{
				oss << m_argument;
			}

		private:
			_Ty m_argument;
		};

		class argument_array : public vector<argument_base*>
		{
		public:
			argument_array() {}
			~argument_array()
			{
				for_each(begin(), end(), [](argument_base* arg_ptr)->void { delete arg_ptr; });
			}
		};

		static void format_item(std::ostringstream& oss, const std::string& item, const argument_array& arguments)
		{
			size_t index = 0;
			long long alignment = 0;
			std::string fmt;
			char* endptr = nullptr;
#if _WIN64
			index = strtoull(&item[0], &endptr, 10);
#else // !_WIN64
			index = strtoul(&item[0], &endptr, 10);
#endif // _WIN64

			if (index < 0 || index >= arguments.size())
				return;

			if (*endptr == ',')
			{
				alignment = strtoll(endptr + 1, &endptr, 10);
				if (alignment > 0)
					oss << std::right << std::setw(alignment);
				else if (alignment < 0)
					oss << std::left << std::setw(-alignment);
			}

			if (*endptr == ':')
				fmt = endptr + 1;

			arguments[index]->format(oss, fmt);
		}

		template <class _Ty>
		static void transfer_to_array(argument_array& arg_array, _Ty arg)
		{
			arg_array.push_back(new argument<_Ty>(arg));
		}

		template <class _Ty, class... _Args>
		static void transfer_to_array(argument_array& arg_array, _Ty arg, _Args&&... args)
		{
			transfer_to_array(arg_array, arg);
			transfer_to_array(arg_array, args...);
		}
	};
}

_WHIP_END

#pragma warning(pop)

#endif // !_WHIP_STRING_OPERATIONS_