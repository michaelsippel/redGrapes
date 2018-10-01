
/**
 * @file rmngr/access/enum.hpp
 */

#pragma once

namespace rmngr
{
namespace access
{

template <
    typename T_Mode
>
struct EnumAccess
{
    T_Mode mode;

    static bool
    is_serial(
        EnumAccess< T_Mode > const & a,
        EnumAcces< T_Mode > const & b
    )
    {
        return a.mode == b.mode;
    }
}; // struct EnumAccess

} // namespace access

} // namespace rmngr

