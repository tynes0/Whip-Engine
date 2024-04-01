#include "whippch.h"
#include "Any.h"

_WHIP_START

namespace aops
{
    bool is_null(const any& data)
    {
        return !data.empty();
    }

    int as_int(const any& data)
    {
        try // try to cast int
        {
            int value = any_cast<int>(data);
            return value;
        }
        catch (bad_any_cast)
        {
            try // try to cast float
            {
                float value = any_cast<float>(data);
                return static_cast<int>(value);
            }
            catch (bad_any_cast)
            {
                try // try to cast double
                {
                    double value = any_cast<double>(data);
                    return static_cast<int>(value);
                }
                catch (bad_any_cast)
                {
                    try // try to cast unsigned int
                    {
                        unsigned int value = any_cast<unsigned int>(data);
                        return static_cast<int>(value);
                    }
                    catch (bad_any_cast)
                    {
                        try // try to cast char
                        {
                            char value = any_cast<char>(data);
                            return static_cast<int>(value);
                        }
                        catch (bad_any_cast)
                        {
                            try // try to cast unsigned char
                            {
                                unsigned char value = any_cast<unsigned char>(data);
                                return static_cast<int>(value);
                            }
                            catch (bad_any_cast)
                            {
                                try // try to cast long
                                {
                                    long value = any_cast<long>(data);
                                    return static_cast<int>(value);
                                }
                                catch (bad_any_cast)
                                {
                                    try // try to cast long long
                                    {
                                        long long value = any_cast<long long>(data);
                                        return static_cast<int>(value);
                                    }
                                    catch (bad_any_cast)
                                    {
                                        try // try to cast short
                                        {
                                            short value = any_cast<short>(data);
                                            return static_cast<int>(value);
                                        }
                                        catch (bad_any_cast)
                                        {
                                            try // try to cast unsigned long
                                            {
                                                unsigned long value = any_cast<unsigned long>(data);
                                                return static_cast<int>(value);
                                            }
                                            catch (bad_any_cast)
                                            {
                                                try // try to cast unsigned long long
                                                {
                                                    unsigned long long value = any_cast<unsigned long long>(data);
                                                    return static_cast<int>(value);
                                                }
                                                catch (bad_any_cast)
                                                {
                                                    try // try to cast unsigned short
                                                    {
                                                        unsigned short value = any_cast<unsigned short>(data);
                                                        return static_cast<int>(value);
                                                    }
                                                    catch (bad_any_cast)
                                                    {
                                                        try // try to cast long double
                                                        {
                                                            long double value = any_cast<long double>(data);
                                                            return static_cast<int>(value);
                                                        }
                                                        catch (bad_any_cast)
                                                        {
                                                            return 0;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    float as_float(const any& data)
    {
        try // try to cast float
        {
            float value = any_cast<float>(data);
            return value;
        }
        catch (bad_any_cast)
        {
            try // try to cast int
            {
                int value = any_cast<int>(data);
                return static_cast<float>(value);
            }
            catch (bad_any_cast)
            {
                try // try to cast double
                {
                    double value = any_cast<double>(data);
                    return static_cast<float>(value);
                }
                catch (bad_any_cast)
                {
                    try // try to cast unsigned int
                    {
                        unsigned int value = any_cast<unsigned int>(data);
                        return static_cast<float>(value);
                    }
                    catch (bad_any_cast)
                    {
                        try // try to cast char
                        {
                            char value = any_cast<char>(data);
                            return static_cast<float>(value);
                        }
                        catch (bad_any_cast)
                        {
                            try // try to cast unsigned char
                            {
                                unsigned char value = any_cast<unsigned char>(data);
                                return static_cast<float>(value);
                            }
                            catch (bad_any_cast)
                            {
                                try // try to cast long
                                {
                                    long value = any_cast<long>(data);
                                    return static_cast<float>(value);
                                }
                                catch (bad_any_cast)
                                {
                                    try // try to cast long long
                                    {
                                        long long value = any_cast<long long>(data);
                                        return static_cast<float>(value);
                                    }
                                    catch (bad_any_cast)
                                    {
                                        try // try to cast short
                                        {
                                            short value = any_cast<short>(data);
                                            return static_cast<float>(value);
                                        }
                                        catch (bad_any_cast)
                                        {
                                            try // try to cast unsigned long
                                            {
                                                unsigned long value = any_cast<unsigned long>(data);
                                                return static_cast<float>(value);
                                            }
                                            catch (bad_any_cast)
                                            {
                                                try // try to cast unsigned long long
                                                {
                                                    unsigned long long value = any_cast<unsigned long long>(data);
                                                    return static_cast<float>(value);
                                                }
                                                catch (bad_any_cast)
                                                {
                                                    try // try to cast unsigned short
                                                    {
                                                        unsigned short value = any_cast<unsigned short>(data);
                                                        return static_cast<float>(value);
                                                    }
                                                    catch (bad_any_cast)
                                                    {
                                                        try // try to cast long double
                                                        {
                                                            long double value = any_cast<long double>(data);
                                                            return static_cast<float>(value);
                                                        }
                                                        catch (bad_any_cast)
                                                        {
                                                            return 0;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    double as_double(const any& data)
    {
        try // try to cast double
        {
            double value = any_cast<double>(data);
            return value;
        }
        catch (bad_any_cast)
        {
            try // try to cast float
            {
                float value = any_cast<float>(data);
                return static_cast<double>(value);
            }
            catch (bad_any_cast)
            {
                try // try to cast int
                {
                    double value = any_cast<int>(data);
                    return static_cast<double>(value);
                }
                catch (bad_any_cast)
                {
                    try // try to cast long double
                    {
                        long double value = any_cast<long double>(data);
                        return static_cast<double>(value);
                    }
                    catch (bad_any_cast)
                    {
                        try // try to cast char
                        {
                            char value = any_cast<char>(data);
                            return static_cast<double>(value);
                        }
                        catch (bad_any_cast)
                        {
                            try // try to cast unsigned char
                            {
                                unsigned char value = any_cast<unsigned char>(data);
                                return static_cast<double>(value);
                            }
                            catch (bad_any_cast)
                            {
                                try // try to cast long
                                {
                                    long value = any_cast<long>(data);
                                    return static_cast<double>(value);
                                }
                                catch (bad_any_cast)
                                {
                                    try // try to cast long long
                                    {
                                        long long value = any_cast<long long>(data);
                                        return static_cast<double>(value);
                                    }
                                    catch (bad_any_cast)
                                    {
                                        try // try to cast unsigned int
                                        {
                                            unsigned int value = any_cast<unsigned int>(data);
                                            return static_cast<double>(value);
                                        }
                                        catch (bad_any_cast)
                                        {
                                            try // try to cast unsigned long
                                            {
                                                unsigned long value = any_cast<unsigned long>(data);
                                                return static_cast<double>(value);
                                            }
                                            catch (bad_any_cast)
                                            {
                                                try // try to cast unsigned long long
                                                {
                                                    unsigned long long value = any_cast<unsigned long long>(data);
                                                    return static_cast<double>(value);
                                                }
                                                catch (bad_any_cast)
                                                {
                                                    try // try to cast short
                                                    {
                                                        short value = any_cast<short>(data);
                                                        return static_cast<double>(value);
                                                    }
                                                    catch (bad_any_cast)
                                                    {
                                                        try // try to cast unsigned short
                                                        {
                                                            unsigned short value = any_cast<unsigned short>(data);
                                                            return static_cast<double>(value);
                                                        }
                                                        catch (bad_any_cast)
                                                        {
                                                            return 0.0;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    char as_char(const any& data)
    {
        try // try to cast char
        {
            char value = any_cast<char>(data);
            return value;
        }
        catch (bad_any_cast)
        {
            try // try to cast unsigned char
            {
                unsigned char value = any_cast<unsigned char>(data);
                return static_cast<char>(value);
            }
            catch (bad_any_cast)
            {
                try // try to cast int
                {
                    int value = any_cast<int>(data);
                    return static_cast<char>(value);
                }
                catch (bad_any_cast)
                {
                    try // try to cast unsigned int
                    {
                        unsigned int value = any_cast<unsigned int>(data);
                        return static_cast<char>(value);
                    }
                    catch (bad_any_cast)
                    {
                        try // try to cast double
                        {
                            double value = any_cast<double>(data);
                            return static_cast<char>(value);
                        }
                        catch (bad_any_cast)
                        {
                            try // try to cast float
                            {
                                float value = any_cast<float>(data);
                                return static_cast<char>(value);
                            }
                            catch (bad_any_cast)
                            {
                                try // try to cast long
                                {
                                    long value = any_cast<long>(data);
                                    return static_cast<char>(value);
                                }
                                catch (bad_any_cast)
                                {
                                    try // try to cast long long
                                    {
                                        long long value = any_cast<long long>(data);
                                        return static_cast<char>(value);
                                    }
                                    catch (bad_any_cast)
                                    {
                                        try // try to cast short
                                        {
                                            short value = any_cast<short>(data);
                                            return static_cast<char>(value);
                                        }
                                        catch (bad_any_cast)
                                        {
                                            try // try to cast unsigned long
                                            {
                                                unsigned long value = any_cast<unsigned long>(data);
                                                return static_cast<char>(value);
                                            }
                                            catch (bad_any_cast)
                                            {
                                                try // try to cast unsigned long long
                                                {
                                                    unsigned long long value = any_cast<unsigned long long>(data);
                                                    return static_cast<char>(value);
                                                }
                                                catch (bad_any_cast)
                                                {
                                                    try // try to cast unsigned short
                                                    {
                                                        unsigned short value = any_cast<unsigned short>(data);
                                                        return static_cast<char>(value);
                                                    }
                                                    catch (bad_any_cast)
                                                    {
                                                        try // try to cast long double
                                                        {
                                                            long double value = any_cast<long double>(data);
                                                            return static_cast<char>(value);
                                                        }
                                                        catch (bad_any_cast)
                                                        {
                                                            return 0;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    unsigned int as_uint(const any& data)
    {
        return 0;
    }

    unsigned char as_uchar(const any& data)
    {
        return 0;
    }

    long as_long(const any& data)
    {
        return 0;
    }

    long long as_long_long(const any& data)
    {
        return 0;
    }

    unsigned long as_ulong(const any& data)
    {
        return 0;
    }

    unsigned long long as_ulong_long(const any& data)
    {
        return 0;
    }

    short as_short(const any& data)
    {
        return 0;
    }

    long double as_long_double(const any& data)
    {
        return 0;
    }

    unsigned short as_ushort(const any& data)
    {
        return 0;
    }
} // namespace aops

_WHIP_END