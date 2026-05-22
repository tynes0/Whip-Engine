#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>

namespace nps
{
	class formatter
	{
	public:
		template <typename... Args>
		[[nodiscard]] static std::string format(const std::string& fmt, Args&&... args)
		{
			if constexpr (sizeof...(Args) == 0)
			{
				return fmt;
			}

			argument_array argArray;
			transfer_to_array(argArray, std::forward<Args>(args)...);
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

		class argument_array : public std::vector<argument_base*>
		{
		public:
			argument_array() {}
			~argument_array()
			{
				std::for_each(begin(), end(), [](argument_base* arg_ptr)->void { delete arg_ptr; });
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

		static void transfer_to_array(argument_array& arg_array) {}

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
